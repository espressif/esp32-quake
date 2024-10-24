#include "bsp/esp-bsp.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "quakedef.h"

#define DEFAULT_VOLUME 50

static esp_codec_dev_handle_t spk_codec_dev;
int snd_inited;

//note: keep >64 and power of 2
#define BUFFER_SIZE		(16*1024)

unsigned char dma_buffer[BUFFER_SIZE];

int dma_rpos;

void CDAudio_get_samps(char *samps, int len_bytes);

#define CHUNKSZ (BUFFER_SIZE/8)
void audio_task(void *param) {
	printf("audio task running\n");
	//simulate DMA to audio; just write the DMA buffer in a circular fashion.
	int16_t mix_buf[CHUNKSZ/2];
	int old_volume=-1;
	while(1) {
		int mainvolume = (int)(volume.value * 100.0);
		if (mainvolume!=old_volume) {
			esp_codec_dev_set_out_vol(spk_codec_dev, mainvolume);
			old_volume=mainvolume;
			printf("Set volume %f\n", mainvolume);
		}
		int cdvol = (int)(bgmvolume.value * 255.0);

		CDAudio_get_samps((char*)mix_buf, CHUNKSZ);
		int16_t *digaudio=(int16_t*)&dma_buffer[dma_rpos];
		//Mix CD audio and digi samples
		for (int i=0; i<CHUNKSZ/2; i++) {
			int a=(((int)mix_buf[i])*cdvol)/256;
//			int a=mix_buf[i];
			int b=digaudio[i];
			int mixed=(a*24)+(b*8); //set mix ratio between cd music and game audio here
			mix_buf[i]=mixed/32;
		}
		dma_rpos=(dma_rpos + CHUNKSZ) % BUFFER_SIZE;
		esp_codec_dev_write(spk_codec_dev, mix_buf, CHUNKSZ);
	}
}

qboolean SNDDMA_Init(void)
{
	bsp_i2c_init();
	bsp_audio_init(NULL);
	spk_codec_dev=bsp_audio_codec_speaker_init();
	esp_codec_dev_set_out_vol(spk_codec_dev, DEFAULT_VOLUME);
	esp_codec_dev_set_out_mute(spk_codec_dev, 0);
	esp_codec_dev_sample_info_t fs = {
		.sample_rate = 44100,
		.channel = 2,
		.bits_per_sample = 16,
	};
	esp_codec_dev_open(spk_codec_dev, &fs);

	shm = &sn;
	shm->splitbuffer = 0;
	shm->speed = 44100;
	shm->samplebits = 16;
	shm->channels = 2;
	shm->soundalive = true;
	shm->samples = sizeof(dma_buffer) / (shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)dma_buffer;
	snd_inited = 1;
	Con_Printf("16 bit stereo sound initialized\n");
	xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 7, NULL, 1);
	return 1;
}

int SNDDMA_GetDMAPos(void) //in (e.g. 16-bit) samples
{
	if (!snd_inited) return (0);
	shm->samplepos=dma_rpos / (shm->samplebits / 8);
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if (snd_inited) {
		snd_inited = 0;
	}
}

//Send sound to device if buffer isn't really the dma buffer
void SNDDMA_Submit(void)
{
}





