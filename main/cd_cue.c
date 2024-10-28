// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "quakedef.h"
#include "esp_attr.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include <fnmatch.h>
#include <sys/types.h>
#include <dirent.h>

/*
Note: The GOG release of the game includes the CDs as cue/gog (actually cue/bin) files
which contains the raw audio as 16-bit signed LE 44100KHz audio. We can simply open those
and use the track timecode to seek to the correct place, then play the track raw.
*/

#define CD_FRAME_SIZE 2352

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

SemaphoreHandle_t player_ctl_mux;

//Borrowed from the Binchunker source code
static long time2frames(char *s)
{
	int mins = 0, secs = 0, frames = 0;
	char *p, *t;
	
	if (!(p = strchr(s, ':'))) return -1;
	*p = '\0';
	mins = atoi(s);
	
	p++;
	if (!(t = strchr(p, ':'))) return -1;
	*t = '\0';
	secs = atoi(p);
	
	t++;
	frames = atoi(t);
	
	return 75 * (mins * 60 + secs) + frames;
}


static const char *cue_candidates[]={
	"game.cue",
	"quake1.cue",
	"quake.cue",
	"*.cue",
	NULL
};


static FILE* open_cuefile(const char *basedir) {
	int n=0;
	while(cue_candidates[n]) {
		DIR *dir=opendir(basedir);
		if (!dir) return NULL;
		struct dirent *de;
		while((de=readdir(dir))!=NULL) {
			if (fnmatch(cue_candidates[n], de->d_name, 0)==0) {
				char fn[MAX_OSPATH];
				sprintf(fn, "%s/%s", basedir, de->d_name);
				FILE *f=fopen(fn, "r");
				if (de) {
					printf("CDAudio: Using cue file %s\n", fn);
					closedir(dir);
					return f;
				}
			}
		}
		closedir(dir);
		n++;
	}
	printf("CDAudio: could not find cue file; not using cd audio.\n");
	return NULL; //nothing found
}

void cd_task(void *param);
RingbufHandle_t ringbuf;
#define RINGBUF_SZ (CD_FRAME_SIZE*32)

int CDAudio_Init(void)
{
	//Need to re-get the basedir from either the defaults or command
	//line. Common.c does this as well but not in an accessible fashion.
	char basedir[MAX_OSPATH];
	int i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1) {
		strcpy (basedir, com_argv[i+1]);
	} else {
		strcpy (basedir, host_parms.basedir);
	}
	int j = strlen (basedir);
	if (j > 0) {
		if ((basedir[j-1] == '\\') || (basedir[j-1] == '/')) {
			basedir[j-1] = 0;
		}
	}

	FILE *f=open_cuefile(basedir);
	if (!f) {
		printf("CDAudio_Init: couldn't find cue file\n");
		return 0;
	}
	if (!f) return 0;
	char buf[1024];
	char binfile[MAX_OSPATH]={0};
	int cur_trk=0;
	int cur_type=0;
	//Hacky parser for .cue lines
	//Note this assumes data tracks are MODE1/2352 (which it should be if there are audio tracks as well)
	while(fgets(buf, sizeof(buf), f)) {
		char *p;
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
				tracks[cur_trk].offset_bytes=time2frames(p)*CD_FRAME_SIZE;
				tracks[cur_trk].type=cur_type;
//				printf("Track %d: %s, offset %d\n", cur_trk, 
//					tracks[cur_trk].type==TYPE_AUDIO?"AUDIO":"OTHER", tracks[cur_trk].offset_bytes);
			}
		}
	}
	fclose(f);

	//Open the bin file
	cdfile=fopen(binfile, "r");
	if (!cdfile) {
		printf("CDAudio_Init: couldn't find bin file %s\n", binfile);
		return 0;
	}
	//Find size of bin file so we know the end of the last track
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
		printf("Track %d: %s, offset %d size %d\n", i, 
					tracks[i].type==TYPE_AUDIO?"AUDIO":"OTHER", 
					tracks[i].offset_bytes,
					tracks[i].length_bytes);
	}

	//Create the ringbuffer where the samples are dumped into and start the cd audio task.
	ringbuf=xRingbufferCreateWithCaps(RINGBUF_SZ, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
	assert(ringbuf);
	player_ctl_mux=xSemaphoreCreateMutex();
	xTaskCreatePinnedToCore(cd_task, "cdaudio", 4096, NULL, 4, NULL, 1);
	return 1;
}


#define STATE_STOPPED 0
#define STATE_PLAYING 1
#define STATE_PLAYNEW 2
#define STATE_SHUTDOWN 3

static int state;
static int cd_track;
static int cd_looping;

//Read samples from the current CD track into a buffer.
void CDAudio_get_samps(char *samps, int len_bytes) {
	size_t recv_size=0;
	if (!ringbuf) {
		//not initialized (yet?) so output silence
		memset(samps, 0, len_bytes);
		return; 
	}
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
	int size=RINGBUF_SZ/4; //we read in chunks this big
	char *item=malloc(size);
	while(state!=STATE_SHUTDOWN) {
		xSemaphoreTake(player_ctl_mux, portMAX_DELAY);
		if (state==STATE_STOPPED) { //or paused
			//output silence
			memset(item, 0, size);
		} else {
			if (state==STATE_PLAYNEW) {
				state=STATE_PLAYING;
				printf("CD: Changing to track %d (looping %d)\n", cd_track, cd_looping);
				fseek(cdfile, tracks[cd_track].offset_bytes, SEEK_SET);
			}
			fread(item, size, 1, cdfile);
			long pos=ftell(cdfile);
			//printf("CD: %d\n", pos);
			assert((pos%CD_FRAME_SIZE)==0);
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
		xSemaphoreGive(player_ctl_mux);
		xRingbufferSend(ringbuf, item, size, portMAX_DELAY);
	}
	fclose(cdfile);
	free(item);
	vTaskDelete(NULL);
}

void CDAudio_Shutdown(void)
{
	state=STATE_SHUTDOWN;
}


void CDAudio_Play(byte track, qboolean looping) {
	if (!cdfile) return;
	xSemaphoreTake(player_ctl_mux, portMAX_DELAY);
	state=STATE_PLAYNEW;
	cd_track=track;
	cd_looping=looping;
	printf("CD: Playing\n");
	xSemaphoreGive(player_ctl_mux);
}


void CDAudio_Stop(void) {
	if (!cdfile) return;
	xSemaphoreTake(player_ctl_mux, portMAX_DELAY);
	state=STATE_STOPPED;
	printf("CD: Stopping\n");
	xSemaphoreGive(player_ctl_mux);
}


void CDAudio_Pause(void) {
	if (!cdfile) return;
	xSemaphoreTake(player_ctl_mux, portMAX_DELAY);
	state=STATE_STOPPED;
	printf("CD: Pausing\n");
	xSemaphoreGive(player_ctl_mux);
}


void CDAudio_Resume(void) {
	if (!cdfile) return;
	xSemaphoreTake(player_ctl_mux, portMAX_DELAY);
	state=STATE_PLAYING;
	printf("CD: Resuming\n");
	xSemaphoreGive(player_ctl_mux);
}


void CDAudio_Update(void) {
}


