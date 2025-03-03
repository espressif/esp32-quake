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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_timer.h"
#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "quakegeneric.h"
#include "soc/mipi_dsi_bridge_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "font_8x16.h"
#include "driver/ppa.h"

#include "quakedef.h"

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;


static uint16_t pal[256];
static uint8_t *cur_pixels;
static uint16_t *lcdbuf[2]={};
static int cur_buf=1;
static int draw_task_quit=0;

static TaskHandle_t draw_task_handle;
static SemaphoreHandle_t drawing_mux;

static int fps_ticks=0;
static int64_t start_time_fps_meas;

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

static void draw_task(void *param) {
	ppa_client_config_t ppa_cfg={
		.oper_type=PPA_OPERATION_SRM,
	};
	ppa_client_handle_t ppa;
	ESP_ERROR_CHECK(ppa_register_client(&ppa_cfg, &ppa));

	uint16_t *rgbfb=heap_caps_calloc(QUAKEGENERIC_RES_X*QUAKEGENERIC_RES_Y, sizeof(uint16_t), MALLOC_CAP_DMA|MALLOC_CAP_SPIRAM);
	assert(rgbfb);
	ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 2, (void**)&lcdbuf[0], (void**)&lcdbuf[1]));

	while(!draw_task_quit) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		xSemaphoreTake(drawing_mux, portMAX_DELAY);
		int64_t start_us = esp_timer_get_time();
		// convert pixels
		uint8_t *p=(uint8_t*)cur_pixels;
		uint16_t *lcdp=rgbfb;
		for (int y=0; y<QUAKEGENERIC_RES_Y; y++) {
			for (int x=0; x<QUAKEGENERIC_RES_X; x++) {
				*lcdp++=pal[*p++];
			}
		}

		//use ppa to scale image
		ppa_srm_oper_config_t op={
			.in={
				.buffer=rgbfb,
				.pic_w=QUAKEGENERIC_RES_X,
				.pic_h=QUAKEGENERIC_RES_Y,
				.block_w=QUAKEGENERIC_RES_X,
				.block_h=QUAKEGENERIC_RES_Y,
				.srm_cm=PPA_SRM_COLOR_MODE_RGB565,
			},
			.out={
				.buffer=lcdbuf[cur_buf],
				.buffer_size=BSP_LCD_V_RES*BSP_LCD_H_RES*sizeof(int16_t),
				.pic_w=BSP_LCD_H_RES,
				.pic_h=BSP_LCD_V_RES,
				.srm_cm=PPA_SRM_COLOR_MODE_RGB565,
			},
			.scale_x=(float)BSP_LCD_H_RES/(float)QUAKEGENERIC_RES_X,
			.scale_y=(float)BSP_LCD_V_RES/(float)QUAKEGENERIC_RES_Y,
			.mode=PPA_TRANS_MODE_BLOCKING,
		};
		ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa, &op));

		xSemaphoreGive(drawing_mux);
		//do a draw to trigger fb flip
		esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, lcdbuf[cur_buf]);
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
	}
}

#define CHAR_W 8
#define CHAR_H 16

static void draw_char(int x, int y, int ch, int fore, int back) {
	//make sure not to draw out of bounds
	if (x<0 || y<0) return;
	if (x+CHAR_W >= BSP_LCD_H_RES) return;
	if (y+CHAR_H >= BSP_LCD_V_RES) return;
	//write character
	uint16_t *fb=lcdbuf[cur_buf];
	for (int py=0; py<CHAR_H; py++) {
		for (int px=0; px<CHAR_W; px++) {
			int pix=fontdata_8x16[ch*CHAR_H+py]&(1<<px);
			int col=pix?fore:back;
			fb[BSP_LCD_H_RES*(y+py)+(x+(7-px))]=pal[col];
		}
	}
}

static void draw_end_screen(char *vgamem) {
	memset(lcdbuf[cur_buf], 0, BSP_LCD_H_RES*BSP_LCD_V_RES*sizeof(uint16_t));

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

	int off_x=(BSP_LCD_H_RES-(80*8))/2;
	int off_y=(BSP_LCD_V_RES-(25*16))/2;

	if (vgamem) {
		for (int y=0; y<25; y++) {
			for (int x=0; x<80; x++) {
				draw_char(x*CHAR_W+off_x, y*CHAR_H+off_y, vgamem[0], vgamem[1]&0xf, (vgamem[1]>>4)&0x7);
				vgamem+=2;
			}
		}
	}
	esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, lcdbuf[cur_buf]);
}


void display_quit() {
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
}


void display_init() {
	//initialize LCD
	const bsp_display_config_t config = {
#if CONFIG_BSP_LCD_TYPE_HDMI
#if CONFIG_BSP_LCD_HDMI_800x600_60HZ
            .hdmi_resolution = BSP_HDMI_RES_800x600,
#elif CONFIG_BSP_LCD_HDMI_1280x720_60HZ
            .hdmi_resolution = BSP_HDMI_RES_1280x720,
#elif CONFIG_BSP_LCD_HDMI_1280x800_60HZ
            .hdmi_resolution = BSP_HDMI_RES_1280x800,
#elif CONFIG_BSP_LCD_HDMI_1920x1080_30HZ
            .hdmi_resolution = BSP_HDMI_RES_1920x1080,
#endif
#else
            .hdmi_resolution = BSP_HDMI_RES_NONE,
#endif
		.dsi_bus = {
			.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
			.lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
		},
	};

	bsp_display_new(&config, &panel_handle, &io_handle);
	esp_lcd_panel_disp_on_off(panel_handle, true);
	bsp_display_brightness_init();
	bsp_display_brightness_set(100);

	drawing_mux=xSemaphoreCreateMutex();
	xTaskCreatePinnedToCore(draw_task, "draw", 4096, NULL, 3, &draw_task_handle, 1);
}


