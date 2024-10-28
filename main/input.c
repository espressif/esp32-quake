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
#include "quakegeneric.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb_hid.h"
#include "hid_keys.h"
#include "quakekeys.h"

//Mapping between USB HID keys and Quake engine keys
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
	[KEY_DOT]='.',
	[KEY_COMMA]=',',
	[KEY_LEFTBRACE]='[',
	[KEY_RIGHTBRACE]=']',
	[KEY_BACKSLASH]='\\',
	[KEY_SEMICOLON]=';',
	[KEY_APOSTROPHE]='\'',
	[KEY_SLASH]='/',
	[KEY_MINUS]='-',
	[KEY_EQUAL]='=',
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


void input_init() {
	xTaskCreatePinnedToCore(usb_hid_task, "usbhid", 4096, NULL, 4, NULL, 1);
}


