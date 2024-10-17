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

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;

void QG_Init(void) {
}

int QG_GetKey(int *down, int *key) {
	//K_TAB, K_ENTER, ...
	*key=0;
	*down=0;
	return 0;
}

void QG_GetJoyAxes(float *axes) {
}

void QG_GetMouseMove(int *x, int *y) {
	*x=0;
	*y=0;
}

void QG_Quit(void) {
}

uint16_t pal[768];
uint8_t *cur_pixels;
uint16_t *lcdbuf[2]={};
int cur_buf=1;

static TaskHandle_t draw_task_handle;
static SemaphoreHandle_t drawing_mux;


void QG_DrawFrame(void *pixels) {
	xSemaphoreTake(drawing_mux, portMAX_DELAY);
	cur_pixels=pixels;
	xSemaphoreGive(drawing_mux);
	xTaskNotifyGive(draw_task_handle);
}

void draw_task(void *param) {
	while(1) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		xSemaphoreTake(drawing_mux, portMAX_DELAY);
		int64_t start_us = esp_timer_get_time();
		// convert pixels
		uint8_t *p=(uint8_t*)cur_pixels;
		uint16_t *lcdp=lcdbuf[cur_buf];
		//Convert LCD buffer addresses to uncached addresses. We don't need to have it pollute cache as
		//we do a linear write to it.
//		lcdp=(uint16_t*)((uint32_t)lcdp+0x40000000U);

		for (int y=0; y<QUAKEGENERIC_RES_Y*QUAKEGENERIC_RES_SCALE; y++) {
			uint8_t *srcline=&p[(y/QUAKEGENERIC_RES_SCALE)*QUAKEGENERIC_RES_X];
			for (int x=0; x<QUAKEGENERIC_RES_X; x++) {
				for (int rep=0; rep<QUAKEGENERIC_RES_SCALE; rep++) {
					*lcdp++=pal[*srcline];
				}
				srcline++;
			}
		}
		xSemaphoreGive(drawing_mux);
		//do a draw to trigger fb flip
		esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 1, 1, lcdbuf[cur_buf]);
		cur_buf=cur_buf?0:1;
		int64_t end_us = esp_timer_get_time();
#if 0
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
	}
}


void quake_task(void *param) {
	char *argv[]={
		"quake", 
		"-basedir", "/sdcard/",
		NULL
	};
	//initialize Quake
	QG_Create(3, argv);

	int64_t oldtime_us = esp_timer_get_time();
	int64_t start_time_fps_meas = esp_timer_get_time();
	int fps_ticks=0;
	while (1) {
		// Run the frame at the correct duration.
		int64_t newtime_us = esp_timer_get_time();
		QG_Tick((double)(newtime_us - oldtime_us)/1000000.0);
		oldtime_us = newtime_us;

		fps_ticks++;
		if (fps_ticks>100) {
			int64_t fpstime=(newtime_us-start_time_fps_meas)/fps_ticks;
			fps_ticks=0;
			start_time_fps_meas = newtime_us;
			printf("Fps: %02f\n", 1000000.0/fpstime);
		}
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

	drawing_mux=xSemaphoreCreateMutex();
	int stack_depth=200*1024;
	
	StaticTask_t *taskbuf=calloc(sizeof(StaticTask_t), 1);
	uint8_t *stackbuf=calloc(stack_depth, 1);
	xTaskCreateStaticPinnedToCore(quake_task, "quake", stack_depth, NULL, 5, (StackType_t*)stackbuf, taskbuf, 0);
	xTaskCreatePinnedToCore(draw_task, "draw", 4096, NULL, 5, &draw_task_handle, 1);
}


