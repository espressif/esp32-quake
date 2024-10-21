#include "bsp/esp-bsp.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "quakedef.h"

#define I2S_NUM 0

#if 0
#define BSP_I2S_SCLK            (GPIO_NUM_29)
#define BSP_I2S_MCLK            (GPIO_NUM_30)
#define BSP_I2S_LCLK            (GPIO_NUM_27)
#define BSP_I2S_DOUT            (GPIO_NUM_26)    // To Codec ES8311
#define BSP_I2S_DSIN            (GPIO_NUM_28)   // From Codec ES8311
#else
#define BSP_I2S_SCLK            (GPIO_NUM_12)
#define BSP_I2S_MCLK            (GPIO_NUM_13)
#define BSP_I2S_LCLK            (GPIO_NUM_10)
#define BSP_I2S_DOUT            (GPIO_NUM_9)    // To Codec ES8311
#define BSP_I2S_DSIN            (GPIO_NUM_11)   // From Codec ES8311
#endif
#define BSP_POWER_AMP_IO        (GPIO_NUM_53)

#define DEFAULT_VOLUME 35

static i2s_chan_handle_t i2s_tx_chan = NULL;
static const audio_codec_data_if_t *i2s_data_if = NULL;
static esp_codec_dev_handle_t spk_codec_dev;
int snd_inited;

//note: keep >64 and power of 2
#define BUFFER_SIZE		(16*1024)

unsigned char dma_buffer[BUFFER_SIZE];

int dma_rpos;

void CDAudio_get_samps(char *samps, int len_bytes);

#define CHUNKSZ (BUFFER_SIZE/8)
void audio_task(void *param) {
	//simulate DMA to audio; just write the DMA buffer in a circular fashion.
	int16_t mix_buf[CHUNKSZ/2];
	while(1) {
		CDAudio_get_samps((char*)mix_buf, CHUNKSZ);
		int16_t *digaudio=(int16_t*)&dma_buffer[dma_rpos];
		//Mix CD audio and digi samples
		for (int i=0; i<CHUNKSZ/2; i++) {
			int a=mix_buf[i];
			int b=digaudio[i];
			int mixed=(a*16)+(b*16); //set mix ratio between cd music and game audio here
			mix_buf[i]=mixed/32;
		}
		dma_rpos=(dma_rpos + CHUNKSZ) % BUFFER_SIZE;
		esp_codec_dev_write(spk_codec_dev, mix_buf, CHUNKSZ);
	}
}

qboolean SNDDMA_Init(void)
{
	bsp_i2c_init();

	i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
	chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
	ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_chan, NULL));
	
	const i2s_std_config_t i2s_cfg={
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
		.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
		.gpio_cfg = {
			.mclk = BSP_I2S_MCLK,
			.bclk = BSP_I2S_SCLK,
			.ws = BSP_I2S_LCLK,
			.dout = BSP_I2S_DOUT,
			.din = BSP_I2S_DSIN,
			.invert_flags = {
				.mclk_inv = false,
				.bclk_inv = false,
				.ws_inv = false,
			}
		}
	};
	ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_chan, &i2s_cfg));
	ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_chan));
	audio_codec_i2s_cfg_t i2s_codec_cfg = {
		.port = I2S_NUM,
		.tx_handle = i2s_tx_chan,
	};
	i2s_data_if = audio_codec_new_i2s_data(&i2s_codec_cfg);

	const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

	audio_codec_i2c_cfg_t i2c_codec_cfg = {
		.port = BSP_I2C_NUM,
		.addr = ES8311_CODEC_DEFAULT_ADDR,
		.bus_handle = bsp_i2c_get_handle(),
	};
	const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_codec_cfg);
	assert(i2c_ctrl_if);

	esp_codec_dev_hw_gain_t gain = {
		.pa_voltage = 5.0,
		.codec_dac_voltage = 3.3,
	};

	es8311_codec_cfg_t es8311_cfg = {
		.ctrl_if = i2c_ctrl_if,
		.gpio_if = gpio_if,
		.codec_mode = ESP_CODEC_DEV_TYPE_OUT,
		.pa_pin = BSP_POWER_AMP_IO,
		.pa_reverted = false,
		.master_mode = false,
		.use_mclk = true,
		.digital_mic = false,
		.invert_mclk = false,
		.invert_sclk = false,
		.hw_gain = gain,
	};
	const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
	assert(es8311_dev);

	esp_codec_dev_cfg_t codec_dev_cfg = {
		.dev_type = ESP_CODEC_DEV_TYPE_OUT,
		.codec_if = es8311_dev,
		.data_if = i2s_data_if,
	};
	spk_codec_dev = esp_codec_dev_new(&codec_dev_cfg);
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
	Con_Printf("16 bit stereo sound initialized\n");
	shm->samplebits = 16;
	shm->channels = 2;
	shm->soundalive = true;
	shm->samples = sizeof(dma_buffer) / (shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)dma_buffer;
	snd_inited = 1;
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





