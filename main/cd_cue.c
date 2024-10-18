#include "quakedef.h"
#include "esp_attr.h"
#include "freertos/ringbuf.h"

/*
Note: The GOG release of the game includes the CDs as cue/gog (actually cue/bin) files
which contains the raw audio as 16-bit signed LE 44100KHz audio. We can simply open those
and use the track timecode to seek to the correct place, then play the track raw.
*/

/*
FILE "game.gog" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    PREGAP 00:02:00
    INDEX 01 13:20:14
  TRACK 03 AUDIO
    INDEX 01 18:32:61
*/

#define TYPE_NONE 0
#define TYPE_AUDIO 1
#define TYPE_OTHER 2

typedef struct {
	int offset_bytes;
	int length_bytes;
	int type;
} track_t;

#define MAX_TRK 32
track_t tracks[MAX_TRK]={0};
FILE *cdfile;

#define CUE "game.cue"

static long time2frames(char *s)
{
	int mins = 0, secs = 0, frames = 0;
	char *p, *t;
	
	if (!(p = strchr(s, ':')))
		return -1;
	*p = '\0';
	mins = atoi(s);
	
	p++;
	if (!(t = strchr(p, ':')))
		return -1;
	*t = '\0';
	secs = atoi(p);
	
	t++;
	frames = atoi(t);
	
	return 75 * (mins * 60 + secs) + frames;
}

void cd_task(void *param);
RingbufHandle_t ringbuf;
#define RINGBUF_SZ (2352*32)

int CDAudio_Init(void)
{
	char    basedir[MAX_OSPATH];
	int i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1) {
		strcpy (basedir, com_argv[i+1]);
	} else {
		strcpy (basedir, host_parms.basedir);
	}
	int j = strlen (basedir);
	if (j > 0) {
		if ((basedir[j-1] == '\\') || (basedir[j-1] == '/'))
			basedir[j-1] = 0;
	}

	char fn[MAX_OSPATH];
	sprintf(fn, "%s/%s", basedir, CUE);
	FILE *f=fopen(fn, "r");
	if (!f) {
		printf("CDAudio_Init: couldn't find cue file %s\n", CUE);
		return 0;
	}
	char buf[1024];
	char binfile[MAX_OSPATH]={0};
	int cur_trk=0;
	int cur_type=0;
	while(fgets(buf, sizeof(buf), f)) {
//		printf("%s\n", buf);
		char *p;
		//Hacky parser for .cue lines
		//Note this assumes data tracks are MODE1/2352 (which it should be if there are audio tracks as well)
		if (p=strstr(buf, "FILE ")) {
			memset(binfile, 0, sizeof(binfile));
			sprintf(binfile, "%s/", basedir);
			while (*p && *p!='"') p++;
			if (*p) {
				*p++; //skip past start of quote
				while (*p && *p!='"') {
					binfile[strlen(binfile)]=*p++;
				}
			}
		} else if (p=strstr(buf, "TRACK")) {
			p+=6; //skip past TRACK
			cur_trk=strtol(p, NULL, 10);
			//figure out if audio or not
			if (strstr(p, "AUDIO")) cur_type=TYPE_AUDIO; else cur_type=TYPE_OTHER;
		} else if (p=strstr(buf, "INDEX")) {
			p+=6; //skip past INDEX
			strtol(p, &p, 10); //index no
			while (*p && *p==' ') *p++; //skip past spaces
			if (cur_trk<MAX_TRK) {
				tracks[cur_trk].offset_bytes=time2frames(p)*2352;
				tracks[cur_trk].type=cur_type;
				printf("Track %d: %s, offset %d\n", cur_trk, 
					tracks[cur_trk].type==TYPE_AUDIO?"AUDIO":"OTHER", tracks[cur_trk].offset_bytes);
			}
		}
	}
	fclose(f);


	cdfile=fopen(binfile, "r");
	if (!cdfile) {
		printf("CDAudio_Init: couldn't find bin file %s\n", binfile);
		return 0;
	}
	fseek(cdfile, 0, SEEK_END);
	long cdsize=ftell(cdfile);
	rewind(cdfile);

	//Figure out the length of the tracks so we can loop
	for (int i=0; i<MAX_TRK; i++) {
		if (tracks[i].type==TYPE_NONE) continue;
		if (i==MAX_TRK-1 || tracks[i+1].type==TYPE_NONE) {
			tracks[i].length_bytes=cdsize-tracks[i].offset_bytes;
		} else {
			tracks[i].length_bytes=tracks[i+1].offset_bytes-tracks[i].offset_bytes;
		}
		printf("Track %d: %s, offset %d size %d\n", cur_trk, 
					tracks[cur_trk].type==TYPE_AUDIO?"AUDIO":"OTHER", 
					tracks[cur_trk].offset_bytes,
					tracks[cur_trk].length_bytes);
		
	}

	ringbuf=xRingbufferCreateWithCaps(RINGBUF_SZ, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
	assert(ringbuf);
	xTaskCreatePinnedToCore(cd_task, "cdaudio", 4096, NULL, 4, NULL, 1);

	return 1;
}


#define STATE_STOPPED 0
#define STATE_PLAYING 1
#define STATE_PLAYNEW 2

//Note: Technically this needs a mux. -JD
static int state;
static int cd_track;
static int cd_looping;


void CDAudio_get_samps(char *samps, int len_bytes) {
	size_t recv_size=0;
	if (!ringbuf) return;
	while (len_bytes) {
		char *item=xRingbufferReceiveUpTo(ringbuf, &recv_size, portMAX_DELAY, len_bytes);
		assert(recv_size<=len_bytes);
		memcpy(samps, item, recv_size);
		vRingbufferReturnItem(ringbuf, item);
		samps+=recv_size;
		len_bytes-=recv_size;
	}
}

void cd_task(void *param) {
	int size=RINGBUF_SZ/4;
	char *item=malloc(size);
	while(1) {
		if (state==STATE_STOPPED) {
			memset(item, 0, size);
		} else {
			if (state==STATE_PLAYNEW) {
				state=STATE_PLAYING;
				fseek(cdfile, tracks[cd_track].offset_bytes, SEEK_SET);
			}
			fread(item, size, 1, cdfile);
			long pos=ftell(cdfile);
			printf("CD: %d\n", pos);
			if (pos > tracks[cd_track].offset_bytes+tracks[cd_track].length_bytes) {
				if (cd_looping) {
					printf("End of track. Looping.\n");
					fseek(cdfile, tracks[cd_track].offset_bytes, SEEK_SET);
				} else {
					cd_track++;
					printf("End of track. Next track %d.\n", cd_track);
					if (tracks[cd_track].type==TYPE_NONE) state=STATE_STOPPED;
				}
			}
		}
		xRingbufferSend(ringbuf, item, size, portMAX_DELAY);
	}
}

void CDAudio_Shutdown(void)
{
	fclose(cdfile);
}


void CDAudio_Play(byte track, qboolean looping) {
	state=STATE_PLAYNEW;
	cd_track=track;
	cd_looping=looping;
}


void CDAudio_Stop(void) {
	state=STATE_STOPPED;
}


void CDAudio_Pause(void) {
	state=STATE_STOPPED;
}


void CDAudio_Resume(void) {
	state=STATE_PLAYING;
}


void CDAudio_Update(void) {
}


