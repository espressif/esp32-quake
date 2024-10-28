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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "errno.h"
#include "hid_ev.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"


static const char *TAG = "example";

QueueHandle_t app_event_queue = NULL;

QueueHandle_t hidev_event_queue = NULL;

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distingiush the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT			- General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST	- HID Host Driver event, such as device connection/disconnection or input report.
 */
typedef enum {
	APP_EVENT = 0,
	APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
	app_event_group_t event_group;
	/* HID Host - Device related info */
	struct {
		hid_host_device_handle_t handle;
		hid_host_driver_event_t event;
		void *arg;
	} hid_host_device;
} app_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
	"NONE",
	"KEYBOARD",
	"MOUSE"
};

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle	 HID Device handle
 * @param[in] event				 HID Host interface event
 * @param[in] arg				 Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
								 const hid_host_interface_event_t event,
								 void *arg)
{
	uint8_t data[64] = { 0 };
	size_t data_length = 0;
	hid_host_dev_params_t dev_params;
	hidev_device_t **hidev_dev_ptr=(hidev_device_t**)arg;
	ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

	switch (event) {
	case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
		ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
																  data,
																  64,
																  &data_length));
		//only parse report if we already have the report descriptors
		if (*hidev_dev_ptr) {
			hidev_parse_report(*hidev_dev_ptr, data, 0);
		}
		break;
	case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
				 hid_proto_name_str[dev_params.proto]);
		ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
		break;
	case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
		ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
				 hid_proto_name_str[dev_params.proto]);
		break;
	default:
		ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
				 hid_proto_name_str[dev_params.proto]);
		break;
	}
}

static void hidev_cb(hid_ev_t *event) {
	xQueueSend(hidev_event_queue, event, pdMS_TO_TICKS(1000));
}


int usb_hid_receive_hid_event(hid_ev_t *ev) {
	return xQueueReceive(hidev_event_queue, ev, 0);
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle	 HID Device handle
 * @param[in] event				 HID Host Device event
 * @param[in] arg				 Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
						   const hid_host_driver_event_t event,
						   void *arg)
{
	hid_host_dev_params_t dev_params;
	ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

	switch (event) {
	case HID_HOST_DRIVER_EVENT_CONNECTED:
		ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
				 hid_proto_name_str[dev_params.proto]);

		if (dev_params.proto!=0) {
			//workaround as we cannot create the hidev without the report, but we cannot get the
			//report before calling hid_host_device_open, which needs the hidev_device.
			hidev_device_t **hidev_ptr=calloc(sizeof(hidev_device_t*), 1);
			const hid_host_device_config_t dev_config = {
				.callback = hid_host_interface_callback,
				.callback_arg = hidev_ptr
			};

			ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
		

		if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
			ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_REPORT));
			if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
				ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
			}
		}

			ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));

			size_t rep_desc_size;
			uint8_t *rep_desc=hid_host_get_report_descriptor(hid_device_handle, &rep_desc_size);
			*hidev_ptr=hidev_device_from_descriptor(rep_desc, rep_desc_size, 0, &hidev_cb);
		}
		break;
	default:
		break;
	}
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void *arg)
{
	const usb_host_config_t host_config = {
		.skip_phy_setup = false,
		.intr_flags = ESP_INTR_FLAG_LEVEL1,
	};

	ESP_ERROR_CHECK(usb_host_install(&host_config));
	xTaskNotifyGive(arg);

	while (true) {
		uint32_t event_flags;
		usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
		// In this example, there is only one client registered
		// So, once we deregister the client, this call must succeed with ESP_OK
		if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
			ESP_ERROR_CHECK(usb_host_device_free_all());
			break;
		}
	}

	ESP_LOGI(TAG, "USB shutdown");
	// Clean up USB Host
	vTaskDelay(10); // Short delay to allow clients clean-up
	ESP_ERROR_CHECK(usb_host_uninstall());
	vTaskDelete(NULL);
}


/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event				HID Device event
 * @param[in] arg				Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
							  const hid_host_driver_event_t event,
							  void *arg)
{
	const app_event_queue_t evt_queue = {
		.event_group = APP_EVENT_HID_HOST,
		// HID Host Device related info
		.hid_host_device.handle = hid_device_handle,
		.hid_host_device.event = event,
		.hid_host_device.arg = arg
	};

	if (app_event_queue) {
		xQueueSend(app_event_queue, &evt_queue, 0);
	}
}

void usb_hid_task(void)
{
	BaseType_t task_created;
	app_event_queue_t evt_queue;

	/*
	* Create usb_lib_task to:
	* - initialize USB Host library
	* - Handle USB Host events
	*/
	task_created = xTaskCreatePinnedToCore(usb_lib_task,
										   "usb_events",
										   4096,
										   xTaskGetCurrentTaskHandle(),
										   5, NULL, 0);
	assert(task_created == pdTRUE);

	// Wait for notification from usb_lib_task to proceed
	ulTaskNotifyTake(false, 1000);

	/*
	* HID host driver configuration
	* - create background task for handling low level event inside the HID driver
	* - provide the device callback to get new HID Device connection event
	*/
	const hid_host_driver_config_t hid_host_driver_config = {
		.create_background_task = true,
		.task_priority = 5,
		.stack_size = 4096,
		.core_id = 0,
		.callback = hid_host_device_callback,
		.callback_arg = NULL
	};

	ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

	// Create queue
	app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
	hidev_event_queue = xQueueCreate(10, sizeof(hid_ev_t));

	ESP_LOGI(TAG, "Waiting for HID Device to be connected");

	while (1) {
		// Wait queue
		if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
			if (evt_queue.event_group == APP_EVENT_HID_HOST) {
				hid_host_device_event(evt_queue.hid_host_device.handle,
									  evt_queue.hid_host_device.event,
									  evt_queue.hid_host_device.arg);
			}
		}
	}

	ESP_LOGI(TAG, "HID Driver uninstall");
	ESP_ERROR_CHECK(hid_host_uninstall());
	xQueueReset(app_event_queue);
	vQueueDelete(app_event_queue);
}
