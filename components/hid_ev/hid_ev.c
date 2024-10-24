/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include "hid_ev.h"
#include "usbhid.h"
#include "usbhid_bsd.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

typedef enum {
	FIELD_NONE=0,
	FIELD_KEY,
	FIELD_KEY_MOD,
	FIELD_JOY_BUTTON,
	FIELD_JOY_AXIS,
	FIELD_JOY_HAT,
	FIELD_MOUSE_BUTTON,
	FIELD_MOUSE_AXIS_X,
	FIELD_MOUSE_AXIS_Y,
	FIELD_MOUSE_WHEEL,
	FIELD_MAX //last item
} field_kind_t;

const char *kind_name[]={
	"FIELD_NONE",
	"FIELD_KEY",
	"FIELD_KEY_MOD",
	"FIELD_JOY_BUTTON",
	"FIELD_JOY_AXIS",
	"FIELD_JOY_HAT",
	"FIELD_MOUSE_BUTTON",
	"FIELD_MOUSE_AXIS_X",
	"FIELD_MOUSE_AXIS_Y",
	"FIELD_MOUSE_WHEEL",
};

typedef enum {
	DEVTYPE_NONE=0,
	DEVTYPE_MOUSE,
	DEVTYPE_KEYBOARD,
	DEVTYPE_JOY
} devtype_t;

typedef struct {
	field_kind_t kind;
	int32_t _usage_page;
	int no;
	int report_id;
	int field_pos;
	int field_len;
	int report_array_ct;
	int min;
	int max;
	union {
		int32_t prev_value;			//used if report_array_ct is 0 or 1
		int32_t *prev_value_arr;	//used if report_array_ct > 1
	}
} hidev_repfield_t;

struct hidev_device_t {
	int device_id;
	int field_count;
	hidev_repfield_t *field;
	hidev_event_cb_t *cb;
};

static devtype_t get_devtype_from_page_usage(int page, int usage) {
	devtype_t ret=DEVTYPE_NONE;
	if (page==HUP_GENERIC_DESKTOP && usage==HUG_POINTER) ret=DEVTYPE_MOUSE;
	if (page==HUP_GENERIC_DESKTOP && usage==HUG_MOUSE) ret=DEVTYPE_MOUSE;
	if (page==HUP_GENERIC_DESKTOP && usage==HUG_KEYBOARD) ret=DEVTYPE_KEYBOARD;
	if (page==HUP_GENERIC_DESKTOP && usage==HUG_KEYPAD) ret=DEVTYPE_KEYBOARD;
	if (page==HUP_GENERIC_DESKTOP && usage==HUG_JOYSTICK) ret=DEVTYPE_JOY;
	if (page==HUP_GENERIC_DESKTOP && usage==HUG_GAME_PAD) ret=DEVTYPE_JOY;
	return ret;
}

static field_kind_t get_kind_from_page_usage(devtype_t devtype, int page, int usage, int rep_size) {
	field_kind_t kind=FIELD_NONE;
	if (devtype==DEVTYPE_MOUSE) {
		if (page==HUP_GENERIC_DESKTOP && usage==HUG_X) kind=FIELD_MOUSE_AXIS_X;
		if (page==HUP_GENERIC_DESKTOP && usage==HUG_Y) kind=FIELD_MOUSE_AXIS_Y;
		if (page==HUP_GENERIC_DESKTOP && usage==HUG_WHEEL) kind=FIELD_MOUSE_WHEEL;
		if (page==HUP_BUTTON) kind=FIELD_MOUSE_BUTTON;
	} else if (devtype==DEVTYPE_KEYBOARD) {
		if (page==HUP_KEYBOARD) {
			if (rep_size==1) {	//1-bit fields are assumed to be modifiers
				kind=FIELD_KEY_MOD;
			} else {
				kind=FIELD_KEY;
			}
		}
	} else if (devtype==DEVTYPE_JOY) {
		if (page==HUP_BUTTON) kind=FIELD_JOY_BUTTON;
		//Note: this kinda depends on X coming before Y... that isn't guaranteed.
		//Guess we should handle this like the mouse...
		if (page==HUP_GENERIC_DESKTOP && usage>=HUG_X && usage<=HUG_RZ) kind=FIELD_JOY_AXIS;
		if (page==HUP_GENERIC_DESKTOP && usage==HUG_HAT_SWITCH) kind=FIELD_JOY_HAT;
	}
	return kind;
}
hidev_device_t *hidev_device_from_descriptor(uint8_t *descriptor, int descriptor_len, int device_id, hidev_event_cb_t *cb) {
	hidev_device_t *dev=calloc(sizeof(hidev_device_t), 1);
	if (!dev) goto err1;
	dev->device_id=device_id;
	dev->cb=cb;
	report_desc_t repdesc=hid_use_report_desc(descriptor, descriptor_len);
	if (repdesc==NULL) goto err1;
	//Go through all report fields to see how many we have
	hid_data_t hiddata=hid_start_parse(repdesc, (1<<hid_input), -1);
	hid_item_t item;
	int field_count=0;
	devtype_t cur_devtype=DEVTYPE_NONE;
	while (hid_get_item(hiddata, &item)) {
		devtype_t dt=get_devtype_from_page_usage(HID_PAGE(item._usage_page), HID_USAGE(item._usage_page));
		if (dt!=DEVTYPE_NONE) cur_devtype=dt;
		field_kind_t kind=get_kind_from_page_usage(cur_devtype, HID_PAGE(item._usage_page), HID_USAGE(item._usage_page), item.report_size);
		if (kind!=FIELD_NONE && item.kind==hid_input && (item.flags&HIO_CONST)==0) field_count+=item.report_count;
	}
	hid_end_parse(hiddata);

	//Allocate field descs
	dev->field=calloc(sizeof(hidev_repfield_t), field_count);
	if (dev->field==NULL) goto err3;
	dev->field_count=field_count;
	//fill in field descs
	hiddata=hid_start_parse(repdesc, (1<<hid_input), -1);
	cur_devtype=DEVTYPE_NONE;
	int kind_ct[FIELD_MAX]={0};
	int i=0;
	while (hid_get_item(hiddata, &item)) {
		devtype_t dt=get_devtype_from_page_usage(HID_PAGE(item._usage_page), HID_USAGE(item._usage_page));
		if (dt!=DEVTYPE_NONE) cur_devtype=dt;
		field_kind_t kind=get_kind_from_page_usage(cur_devtype, HID_PAGE(item._usage_page), HID_USAGE(item._usage_page), item.report_size);
		if (kind!=FIELD_NONE && item.kind==hid_input && (item.flags&HIO_CONST)==0) {
			dev->field[i].kind=kind;
			dev->field[i].no=kind_ct[kind];
			kind_ct[kind]++;
			dev->field[i]._usage_page=item._usage_page;
			dev->field[i].report_id=item.report_ID;
			dev->field[i].report_array_ct=item.report_count;
			dev->field[i].field_pos=item.pos;
			dev->field[i].field_len=item.report_size;
			dev->field[i].min=item.logical_minimum;
			dev->field[i].max=item.logical_maximum;
			if (item.report_count>1) {
				dev->field[i].prev_value_arr=calloc(item.report_count, sizeof(uint32_t));
			}
			i++;
		}
	}
	hid_end_parse(hiddata);
	hid_dispose_report_desc(repdesc);
	return dev;

	//todo: we leak field memory
err3:
	free(dev);
	hid_dispose_report_desc(repdesc);
err1:
	free(dev);
	return NULL;
}

static int32_t get_field_data_array_item(const void *p, hidev_repfield_t *field, int item) {
	assert((item==0 && field->report_array_ct==0) || (item < field->report_array_ct));
	uint32_t hpos=field->field_pos;
	uint32_t hsize=field->field_len;
	hpos += hsize * item;
	const uint8_t *buf = p;

	/* Range check and limit */
	if (hsize == 0) return (0);
	if (hsize > 32) hsize = 32;

	int offs = hpos / 8;
	int end = (hpos + hsize) / 8 - offs;
	uint32_t data = 0;
	for (int i = 0; i <= end; i++) {
		data |= buf[offs + i] << (i*8);
	}

	/* Correctly shift down data */
	data >>= hpos % 8;
	hsize = 32 - hsize;

	/* Mask and sign extend in one */
	if ((field->min < 0) || (field->max < 0)) {
		data = (int32_t)((int32_t)data << hsize) >> hsize;
	} else {
		data = (uint32_t)((uint32_t)data << hsize) >> hsize;
	}
	return data;
}

static int32_t get_field_data(const void *p, hidev_repfield_t *field) {
	return get_field_data_array_item(p, field, 0);
}

static void send_event_for(hidev_device_t *dev, hidev_repfield_t *field, int32_t data) {
	hid_ev_t ev={0};
	ev.device_id=dev->device_id;
	ev.no=field->no;
	if (field->kind==FIELD_KEY_MOD) {
		ev.type=data?HIDEV_EVENT_KEY_DOWN:HIDEV_EVENT_KEY_UP;
		ev.key.keycode=HID_USAGE(field->_usage_page);
	} else if (field->kind==FIELD_JOY_BUTTON) {
		ev.type=data?HIDEV_EVENT_JOY_BUTTONDOWN:HIDEV_EVENT_JOY_BUTTONUP;
	} else if (field->kind==FIELD_JOY_AXIS) {
		ev.type=HIDEV_EVENT_JOY_AXIS;
		ev.joyaxis.pos=((data-field->min)*65536ULL)/(field->max-field->min+1)-32768;
	} else if (field->kind==FIELD_JOY_HAT) {
		ev.type=HIDEV_EVENT_JOY_HAT;
		ev.joyhat.pos=data;
	} else if (field->kind==FIELD_MOUSE_BUTTON) {
		ev.type=data?HIDEV_EVENT_MOUSE_BUTTONDOWN:HIDEV_EVENT_MOUSE_BUTTONUP;
	} else if (field->kind==FIELD_MOUSE_AXIS_X || field->kind==FIELD_MOUSE_AXIS_Y) {
		//We don't parse this here as we need to know both the x- and y-axis for the event
	} else if (field->kind==FIELD_MOUSE_WHEEL) {
		ev.type=HIDEV_EVENT_MOUSE_WHEEL;
		ev.mouse_wheel.d=data;
	}
	dev->cb(&ev);
}

static void send_event_for_key(hidev_device_t *dev, hidev_repfield_t *field, int32_t data, int pressed) {
	hid_ev_t ev={0};
	ev.device_id=dev->device_id;
	ev.no=field->no;
	ev.type=pressed?HIDEV_EVENT_KEY_DOWN:HIDEV_EVENT_KEY_UP;
	ev.key.keycode=data;
	dev->cb(&ev);
}

static void send_event_for_mouse_motion(hidev_device_t *dev, hidev_repfield_t *field, int32_t data_x, int32_t data_y) {
	hid_ev_t ev={0};
	ev.device_id=dev->device_id;
	ev.no=field->no;
	ev.type=HIDEV_EVENT_MOUSE_MOTION;
	ev.mouse_motion.dx=data_x;
	ev.mouse_motion.dy=data_y;
	dev->cb(&ev);
}

void hidev_parse_report(hidev_device_t *dev, uint8_t *report, int report_id) {
	if (!dev) return;
	int mouse_x=0;
	int mouse_changed=0;
	for (int i=0; i<dev->field_count; i++) {
		if (dev->field[i].report_id==report_id) {
			int32_t data=get_field_data(report, &dev->field[i]);
			//Handle mouse axes differently as we need to combine x and y into one motion event
			if (dev->field[i].kind==FIELD_MOUSE_AXIS_X) {
				mouse_x=data;
				if (data!=dev->field[i].prev_value) mouse_changed=1;
			} else if (dev->field[i].kind==FIELD_MOUSE_AXIS_Y) {
				if (data!=dev->field[i].prev_value) mouse_changed=1;
				if (mouse_changed) send_event_for_mouse_motion(dev, &dev->field[i], mouse_x, data);
			} else if (dev->field[i].kind==FIELD_KEY) {
				//This is usually an array of entries. Figure out the difference between it and the
				//previous values.
				uint32_t *prev_vals=dev->field[i].prev_value_arr;
				int n=dev->field[i].report_array_ct;
				if (n<2) prev_vals=&dev->field[i].prev_value; //fallback in case of no array
				for (int j=0; j<n; j++) {
					uint32_t data=get_field_data_array_item(report, &dev->field[i], j);
					uint32_t prev_data=prev_vals[j];
					int newly_pressed=(data!=0);
					int newly_released=(prev_data!=0);
					if (newly_pressed || newly_released) {
						//see if it's really *newly* pressed or released
						for (int k=0; k<n; k++) {
							uint32_t other_data=get_field_data_array_item(report, &dev->field[i], k);
							uint32_t other_prev_data=prev_vals[k];
							if (data == other_prev_data) newly_pressed=0; //was previously also pressed
							if (prev_data == other_data) newly_released=0; //is still pressed
						}
					}
					if (newly_pressed) send_event_for_key(dev, &dev->field[i], data, 1);
					if (newly_released) send_event_for_key(dev, &dev->field[i], prev_data, 0);
				}
				//set prev_vals for next time
				for (int j=0; j<n; j++) {
					prev_vals[j]=get_field_data_array_item(report, &dev->field[i], j);
				}
			} else {
				//generic entry
				if (data!=dev->field[i].prev_value) {
					send_event_for(dev, &dev->field[i], data);
					dev->field[i].prev_value=data;
				}
			}
		}
	}
}
