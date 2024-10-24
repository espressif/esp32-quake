#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "quakegeneric.h"
#include "soc/mipi_dsi_bridge_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "quakekeys.h"
#include "eth_connect.h"
#include "font_8x16.h"
#include "input.h"

#include "quakedef.h"

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;

#define LCD_W 1024
#define LCD_H 600

void QG_Init(void) {
}

#if QUAKEGENERIC_RES_SCALE==2
uint32_t pal[768];
#else
uint16_t pal[768];
#endif
uint8_t *cur_pixels;
uint16_t *lcdbuf[2]={};
int cur_buf=1;
int draw_task_quit=0;

static TaskHandle_t draw_task_handle;
static SemaphoreHandle_t drawing_mux;

int fps_ticks=0;
int64_t start_time_fps_meas;

void QG_DrawFrame(void *pixels) {
	xSemaphoreTake(drawing_mux, portMAX_DELAY);
	cur_pixels=pixels;
	xSemaphoreGive(drawing_mux);
	xTaskNotifyGive(draw_task_handle);
	fps_ticks++;
	if (fps_ticks>100) {
		int64_t newtime_us=esp_timer_get_time();
		int64_t fpstime=(newtime_us-start_time_fps_meas)/fps_ticks;
		fps_ticks=0;
		start_time_fps_meas = newtime_us;
		printf("Fps: %02f\n", 1000000.0/fpstime);
	}
}

void draw_task(void *param) {
	while(!draw_task_quit) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		xSemaphoreTake(drawing_mux, portMAX_DELAY);
		int64_t start_us = esp_timer_get_time();
		// convert pixels
		uint8_t *p=(uint8_t*)cur_pixels;
		uint16_t *lcdp=lcdbuf[cur_buf];

#if QUAKEGENERIC_RES_SCALE==2
		//If we scale by 2, we can optimize by writing 32 bits at a time.
		for (int y=0; y<QUAKEGENERIC_RES_Y*QUAKEGENERIC_RES_SCALE; y++) {
			uint8_t *srcline=&p[(y/QUAKEGENERIC_RES_SCALE)*QUAKEGENERIC_RES_X];
			uint32_t *dst=(uint32_t*)&lcdp[LCD_W*y];
			for (int x=0; x<QUAKEGENERIC_RES_X; x++) {
				*dst++=pal[*srcline++];
			}
		}
#else
		//generic random scaling
		for (int y=0; y<QUAKEGENERIC_RES_Y*QUAKEGENERIC_RES_SCALE; y++) {
			uint8_t *srcline=&p[(y/QUAKEGENERIC_RES_SCALE)*QUAKEGENERIC_RES_X];
			uint16_t *dst=&lcdp[LCD_W*y];
			for (int x=0; x<QUAKEGENERIC_RES_X; x++) {
				for (int rep=0; rep<QUAKEGENERIC_RES_SCALE; rep++) {
					*dst++=pal[*srcline];
				}
				srcline++;
			}
		}
#endif
		xSemaphoreGive(drawing_mux);
		//do a draw to trigger fb flip
		esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_W, LCD_H, lcdbuf[cur_buf]);
		cur_buf=cur_buf?0:1;
		int64_t end_us = esp_timer_get_time();
#if 0
		//Shows the maximum FPS possible given the lcd frame drawing code
		printf("LCD Fps: %02f\n", 1000000.0/(end_us-start_us));
#endif
	}
}

void QG_SetPalette(unsigned char palette[768]) {
	unsigned char *p=palette;
	for (int i=0; i<256; i++) {
		//convert to rgb565
		int b=(*p++)>>3;
		int g=(*p++)>>2;
		int r=(*p++)>>3;
		pal[i]=r+(g<<5)+(b<<11);
#if QUAKEGENERIC_RES_SCALE==2
		pal[i]=pal[i]|(pal[i]<<16);
#endif
	}
}


void draw_char(int x, int y, int ch, int fore, int back) {
	uint16_t *fb=lcdbuf[cur_buf];
	for (int py=0; py<16; py++) {
		for (int px=0; px<8; px++) {
			int pix=fontdata_8x16[ch*16+py]&(1<<px);
			int col=pix?fore:back;
			fb[LCD_W*(y+py)+(x+(7-px))]=pal[col];
		}
	}
}


void draw_end_screen(char *vgamem) {
	memset(lcdbuf[cur_buf], 0, LCD_W*LCD_H*sizeof(uint16_t));

	const unsigned char pal[768]={ //EGA pallette; b, g, r
		0x00, 0x00, 0x00,
		0x00, 0x00, 0xaa,
		0x00, 0xaa, 0x00,
		0x00, 0xaa, 0xaa,
		0xaa, 0x00, 0x00,
		0xaa, 0x00, 0xaa,
		0xaa, 0x55, 0x00,
		0xaa, 0xaa, 0xaa,
		0x55, 0x55, 0x55,
		0x55, 0x55, 0xff,
		0x55, 0xff, 0x55,
		0x55, 0xff, 0xff,
		0xff, 0x55, 0x55,
		0xff, 0x55, 0xff,
		0xff, 0xff, 0x55,
		0xff, 0xff, 0xff
	};
	QG_SetPalette(pal);

	int off_x=(LCD_W-(80*8))/2;
	int off_y=(LCD_H-(25*16))/2;

	if (vgamem) {
		for (int y=0; y<25; y++) {
			for (int x=0; x<80; x++) {
				draw_char(x*8+off_x, y*16+off_y, vgamem[0], vgamem[1]&0xf, (vgamem[1]>>4)&0x7);
				vgamem+=2;
			}
		}
	}
	esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_W, LCD_H, lcdbuf[cur_buf]);
}

void QG_Quit(void) {
	draw_task_quit=1;
	xSemaphoreGive(drawing_mux);
	vTaskDelay(pdMS_TO_TICKS(30)+1);
	//we can now use the framebuffer to draw the end screen
	unsigned char *d;
	if (registered.value)
		d = COM_LoadHunkFile ("end2.bin"); 
	else
		d = COM_LoadHunkFile ("end1.bin"); 
	draw_end_screen(d);

	while(1) vTaskDelay(100);
}


void quake_task(void *param) {
	char *argv[]={
		"quake", 
		"-basedir", "/sdcard/",
		NULL
	};
	//initialize Quake
	QG_Create(3, argv);

	start_time_fps_meas = esp_timer_get_time();
	int64_t oldtime_us = esp_timer_get_time();
	while (1) {
		// Run the frame at the correct duration.
		int64_t newtime_us = esp_timer_get_time();
		QG_Tick((double)(newtime_us - oldtime_us)/1000000.0);
		oldtime_us = newtime_us;
	}
}

void app_main() {
	bsp_sdcard_mount();
	//initialize LCD
	bsp_display_new(NULL, &panel_handle, &io_handle);
	esp_lcd_panel_disp_on_off(panel_handle, true);
	bsp_display_brightness_init();
	bsp_display_brightness_set(100);

	ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 2, (void**)&lcdbuf[0], (void**)&lcdbuf[1]));

	ethernet_connect();

	input_init();

	drawing_mux=xSemaphoreCreateMutex();
	int stack_depth=200*1024;

	StaticTask_t *taskbuf=calloc(sizeof(StaticTask_t), 1);
	uint8_t *stackbuf=calloc(stack_depth, 1);
	xTaskCreateStaticPinnedToCore(quake_task, "quake", stack_depth, NULL, 2, (StackType_t*)stackbuf, taskbuf, 0);
	xTaskCreatePinnedToCore(draw_task, "draw", 4096, NULL, 3, &draw_task_handle, 1);
}


