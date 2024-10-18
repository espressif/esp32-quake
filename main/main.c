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
#include "usb_hid.h"
#include "usb_hid_keys.h"
#include "quakekeys.h"

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;

void QG_Init(void) {
}

const uint16_t key_conv_tab[]={
	[KEY_TAB]=K_TAB, 
	[KEY_ENTER]=K_ENTER, 
	[KEY_ESC]=K_ESCAPE, 
	[KEY_SPACE]=K_SPACE, 
	[KEY_BACKSPACE]=K_BACKSPACE, 
	[KEY_UP]=K_UPARROW, 
	[KEY_DOWN]=K_DOWNARROW, 
	[KEY_LEFT]=K_LEFTARROW, 
	[KEY_RIGHT]=K_RIGHTARROW, 
	[KEY_LEFTALT]=K_ALT, 
	[KEY_RIGHTALT]=K_ALT, 
	[KEY_LEFTCTRL]=K_CTRL, 
	[KEY_RIGHTCTRL]=K_CTRL, 
	[KEY_LEFTSHIFT]=K_SHIFT, 
	[KEY_RIGHTSHIFT]=K_SHIFT, 
	[KEY_F1]=K_F1, 
	[KEY_F2]=K_F2, 
	[KEY_F3]=K_F3, 
	[KEY_F4]=K_F4, 
	[KEY_F5]=K_F5, 
	[KEY_F6]=K_F6, 
	[KEY_F7]=K_F7, 
	[KEY_F8]=K_F8, 
	[KEY_F9]=K_F9, 
	[KEY_F10]=K_F10, 
	[KEY_F11]=K_F11, 
	[KEY_F12]=K_F12, 
	[KEY_INSERT]=K_INS, 
	[KEY_DELETE]=K_DEL, 
	[KEY_PAGEDOWN]=K_PGDN, 
	[KEY_PAGEUP]=K_PGUP, 
	[KEY_HOME]=K_HOME, 
	[KEY_END]=K_END,
	[KEY_A]='a',
	[KEY_B]='b',
	[KEY_C]='c',
	[KEY_D]='d',
	[KEY_E]='e',
	[KEY_F]='f',
	[KEY_G]='g',
	[KEY_H]='h',
	[KEY_I]='i',
	[KEY_J]='j',
	[KEY_K]='k',
	[KEY_L]='l',
	[KEY_M]='m',
	[KEY_N]='n',
	[KEY_O]='o',
	[KEY_P]='p',
	[KEY_Q]='q',
	[KEY_R]='r',
	[KEY_S]='s',
	[KEY_T]='t',
	[KEY_U]='u',
	[KEY_V]='v',
	[KEY_W]='w',
	[KEY_X]='x',
	[KEY_Y]='y',
	[KEY_Z]='z',
	[KEY_1]='1',
	[KEY_2]='2',
	[KEY_3]='3',
	[KEY_4]='4',
	[KEY_5]='5',
	[KEY_6]='6',
	[KEY_7]='7',
	[KEY_8]='8',
	[KEY_9]='9',
	[KEY_0]='0',
	[KEY_GRAVE]='`',
};

static int mouse_dx=0, mouse_dy=0;

int QG_GetKey(int *down, int *key) {
	*key=0;
	*down=0;
	hid_ev_t ev;
	int ret=usb_hid_receive_hid_event(&ev);
	if (!ret) return 0;
	if (ev.type==HIDEV_EVENT_KEY_DOWN || ev.type==HIDEV_EVENT_KEY_UP) {
		*down=(ev.type==HIDEV_EVENT_KEY_DOWN)?1:0;
		if (ev.key.keycode < sizeof(key_conv_tab)/sizeof(key_conv_tab[0])) {
			*key=key_conv_tab[ev.key.keycode];
		}
		return 1;
	} else if (ev.type==HIDEV_EVENT_MOUSE_BUTTONDOWN || ev.type==HIDEV_EVENT_MOUSE_BUTTONUP) {
		*key=K_MOUSE1+ev.no;
		*down=(ev.type==HIDEV_EVENT_MOUSE_BUTTONDOWN)?1:0;
		return 1;
	} else if (ev.type==HIDEV_EVENT_MOUSE_MOTION) {
		mouse_dx+=ev.mouse_motion.dx;
		mouse_dy+=ev.mouse_motion.dy;
	} else if (ev.type==HIDEV_EVENT_MOUSE_WHEEL) {
		int d=ev.mouse_wheel.d;
		if (d!=0) {
			*key=(d<0)?K_MWHEELUP:K_MWHEELDOWN;
			*down=1;
			return 1;
		}
	}
	return 0;
}

void QG_GetJoyAxes(float *axes) {
}

void QG_GetMouseMove(int *x, int *y) {
	*x=mouse_dx;
	*y=mouse_dy;
	mouse_dx=0;
	mouse_dy=0;
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
		//Nope - turns out this is slower.
//		lcdp=(uint16_t*)((uint32_t)lcdp+0x40000000U);

		for (int y=0; y<QUAKEGENERIC_RES_Y*QUAKEGENERIC_RES_SCALE; y++) {
			uint8_t *srcline=&p[(y/QUAKEGENERIC_RES_SCALE)*QUAKEGENERIC_RES_X];
			uint16_t *dst=&lcdp[1024*y];
			for (int x=0; x<QUAKEGENERIC_RES_X; x++) {
				for (int rep=0; rep<QUAKEGENERIC_RES_SCALE; rep++) {
					*dst++=pal[*srcline];
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
	xTaskCreateStaticPinnedToCore(quake_task, "quake", stack_depth, NULL, 2, (StackType_t*)stackbuf, taskbuf, 0);
	xTaskCreatePinnedToCore(draw_task, "draw", 4096, NULL, 3, &draw_task_handle, 1);
	xTaskCreatePinnedToCore(usb_hid_task, "usbhid", 4096, NULL, 4, NULL, 1);
}


