#include "usb_class_driver.h"
#include "usb_utils.h"

typedef struct {
	usb_host_client_handle_t client_hdl;
	uint8_t dev_addr;
	usb_device_handle_t dev_hdl;
	uint32_t actions;
} class_driver_t;

static const char *TAG = "PRINTER_DRIVER";

//Allocate a USB transfer
usb_transfer_t *transfer;
usb_transfer_t *transfer_in;

const usb_device_desc_t *dev_desc;

const uint8_t endpoint_addr = 0x03;
const uint8_t interface_number = 0x01;

class_driver_t driver_obj = { 0 };

static bool isSetupDone = false;
static bool isPrinterError = false;
static bool isPrinterConnected = false;

bool is_printer_connected() {
	return isPrinterConnected;
}

bool is_setup_done() {
	return isSetupDone;
}

bool is_printer_error() {
	return isPrinterError;
}

void set_setup_done(bool status) {
	isSetupDone = status;
}

void set_printer_error(bool status) {
	isPrinterError = status;
}

void set_printer_connected(bool status) {
	isPrinterConnected = status;
}

esp_err_t transfer_cmd(uint8_t *data, int size) {
	transfer->num_bytes = size;

	memcpy(transfer->data_buffer, data, transfer->num_bytes);

	esp_err_t err = usb_host_transfer_submit(transfer);

	vTaskDelay(pdMS_TO_TICKS(250));

	if (err != ESP_OK) {
		ESP_LOGI(TAG, "usb_host_transfer_submit Out fail: %s",
				esp_err_to_name(err));

		set_printer_error(true);
	} else {
		set_printer_error(false);
	}

	return err;
}

static void transfer_cb(usb_transfer_t *transfer) {

	class_driver_t *driver_obj = (class_driver_t*) transfer->context;

	if (transfer->status != 0) {
		set_setup_done(false);
		set_printer_error(true);
	} else {
		set_setup_done(true);
		set_printer_error(false);
	}

	printf("Transfer status %d, actual number of bytes transferred %d\n",
			transfer->status, transfer->actual_num_bytes);

	ESP_LOG_BUFFER_HEX(TAG, transfer->data_buffer, transfer->actual_num_bytes);

	if (driver_obj->dev_hdl == transfer->device_handle) {
		int in_xfer = transfer->bEndpointAddress
				& USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;

		if (transfer->status == 0) {
			if (in_xfer) {
				uint8_t *const p = transfer->data_buffer;
				for (int i = 0; i < transfer->actual_num_bytes; i++) {
					ESP_LOGI(TAG, "printer in: %02x", p[i]);
				}
				esp_err_t err = usb_host_transfer_submit(transfer);
				if (err != ESP_OK) {
					ESP_LOGI(TAG, "usb_host_transfer_submit In fail: %x", err);
				}
			}
		} else {
			ESP_LOGI(TAG, "transfer->status %d", transfer->status);
		}
	}
}

static void client_event_cb(const usb_host_client_event_msg_t *event_msg,
		void *arg) {
	driver_obj = *(class_driver_t*) (arg);

	ESP_LOGE(TAG, "<<< client_event_cb [event_msg->event: %d] >>>\n\n",
			event_msg->event);

	switch (event_msg->event) {
	case USB_HOST_CLIENT_EVENT_NEW_DEV:
		if (driver_obj.dev_addr == 0) {
			driver_obj.dev_addr = event_msg->new_dev.address;
			//Open the device next
			driver_obj.actions |= ACTION_OPEN_DEV;
		}
		break;
	case USB_HOST_CLIENT_EVENT_DEV_GONE:
		if (driver_obj.dev_hdl != NULL) {
			//Cancel any other actions and close the device next
			driver_obj.actions = ACTION_CLOSE_DEV;
		}
		break;
	default:
		//Should never occur
		abort();
	}
}

static void action_open_dev(class_driver_t *driver_obj) {
	assert(driver_obj->dev_addr != 0);
	ESP_LOGI(TAG, "Opening device at address %d", driver_obj->dev_addr);
	ESP_ERROR_CHECK(
			usb_host_device_open(driver_obj->client_hdl, driver_obj->dev_addr,
					&driver_obj->dev_hdl));
	//Get the device's information next
	driver_obj->actions &= ~ACTION_OPEN_DEV;
	driver_obj->actions |= ACTION_GET_DEV_INFO;
}

static void action_get_info(class_driver_t *driver_obj) {
	assert(driver_obj->dev_hdl != NULL);
	ESP_LOGI(TAG, "Getting device information");
	usb_device_info_t dev_info;
	ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
	ESP_LOGI(TAG, "\t%s speed",
			(dev_info.speed == USB_SPEED_LOW) ? "Low" : "Full");
	ESP_LOGI(TAG, "\tbConfigurationValue %d", dev_info.bConfigurationValue);
	//Todo: Print string descriptors

	//Get the device descriptor next
	driver_obj->actions &= ~ACTION_GET_DEV_INFO;
	driver_obj->actions |= ACTION_GET_DEV_DESC;
}

static void action_get_dev_desc(class_driver_t *driver_obj) {
	assert(driver_obj->dev_hdl != NULL);
	ESP_LOGI(TAG, "Getting device descriptor");
	//const usb_device_desc_t *dev_desc;
	ESP_ERROR_CHECK(
			usb_host_get_device_descriptor(driver_obj->dev_hdl, &dev_desc));
	usb_print_device_descriptor(dev_desc);
	//Get the device's config descriptor next
	driver_obj->actions &= ~ACTION_GET_DEV_DESC;
	driver_obj->actions |= ACTION_GET_CONFIG_DESC;
}

static void action_get_config_desc(class_driver_t *driver_obj) {
	assert(driver_obj->dev_hdl != NULL);
	ESP_LOGI(TAG, "Getting config descriptor");
	const usb_config_desc_t *config_desc;
	ESP_ERROR_CHECK(
			usb_host_get_active_config_descriptor(driver_obj->dev_hdl,
					&config_desc));
	usb_print_config_descriptor(config_desc, NULL);
	//Get the device's string descriptors next
	driver_obj->actions &= ~ACTION_GET_CONFIG_DESC;
	driver_obj->actions |= ACTION_GET_STR_DESC;
}

static void action_get_str_desc(class_driver_t *driver_obj) {
	assert(driver_obj->dev_hdl != NULL);
	usb_device_info_t dev_info;
	ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
	if (dev_info.str_desc_manufacturer) {
		ESP_LOGI(TAG, "Getting Manufacturer string descriptor");
		usb_print_string_descriptor(dev_info.str_desc_manufacturer);
	}
	if (dev_info.str_desc_product) {
		ESP_LOGI(TAG, "Getting Product string descriptor");
		usb_print_string_descriptor(dev_info.str_desc_product);
	}
	if (dev_info.str_desc_serial_num) {
		ESP_LOGI(TAG, "Getting Serial Number string descriptor");
		usb_print_string_descriptor(dev_info.str_desc_serial_num);
	}
	//Nothing to do until the device disconnects
	driver_obj->actions &= ~ACTION_GET_STR_DESC;
}

static void action_close_dev(class_driver_t *driver_obj) {
//	ESP_ERROR_CHECK(
	usb_host_device_close(driver_obj->client_hdl, driver_obj->dev_hdl);
	//);
	driver_obj->dev_hdl = NULL;
	driver_obj->dev_addr = 0;
	//We need to exit the event handler loop
	driver_obj->actions &= ~ACTION_CLOSE_DEV;
	driver_obj->actions |= ACTION_EXIT;
}

static void transfer_alloc(class_driver_t *driver_obj) {
	usb_host_transfer_alloc(64, 0, &transfer_in);

	transfer_in->device_handle = driver_obj->dev_hdl;
	transfer_in->bEndpointAddress = 0x81;
	transfer_in->callback = transfer_cb;
	transfer_in->context = NULL;
	transfer_in->num_bytes = 64;

	usb_host_transfer_submit(transfer_in);

	usb_host_transfer_alloc(1024, 0, &transfer);
}

void class_driver_task(void *arg) {
	SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t) arg;

	set_setup_done(false);

	//Wait until daemon task has installed USB Host Library
	xSemaphoreTake(signaling_sem, portMAX_DELAY);

	ESP_LOGI(TAG, "Registering Client");

	usb_host_client_config_t client_config = { .is_synchronous = false, //Synchronous clients currently not supported. Set this to false
			.max_num_event_msg = CLIENT_NUM_EVENT_MSG, .async = {
					.client_event_callback = client_event_cb, .callback_arg =
							(void*) &driver_obj, }, };

	while (usb_host_client_register(&client_config, &driver_obj.client_hdl)
			!= ESP_OK) {
		ESP_LOGE(TAG, "Registering Host Client...");
		vTaskDelay(pdMS_TO_TICKS(50));
	}

	transfer_alloc(&driver_obj);

	while (1) {
		if (driver_obj.actions == 0) {
			while (usb_host_client_handle_events(driver_obj.client_hdl,
			portMAX_DELAY) != ESP_OK) {
				vTaskDelay(pdMS_TO_TICKS(50));
			}
		} else {

			set_printer_connected(true);

			if (driver_obj.actions & ACTION_OPEN_DEV) {
				action_open_dev(&driver_obj);
			}
			if (driver_obj.actions & ACTION_GET_DEV_INFO) {
				action_get_info(&driver_obj);
			}
			if (driver_obj.actions & ACTION_GET_DEV_DESC) {
				action_get_dev_desc(&driver_obj);

				if (dev_desc != nullptr)
                {
                    int bInterfaceNumber = 0;
                    int bAlternateSetting = 0;

                    // TP-B7AI
                    // idVendor 0x456
                    // idProduct 0x808
                    // bInterfaceNumber 0
                    // bAlternateSetting 0

                    // KP-1025
                    // idVendor 0x483
                    // idProduct 0x5720
                    // bInterfaceNumber 1
                    // bAlternateSetting 0

                    // POS-5890K
                    // idVendor 0x416
                    // idProduct 0x5011
                    // bInterfaceNumber 0
                    // bAlternateSetting 0


                    if(dev_desc->idVendor == 0x456 && dev_desc->idProduct == 0x808){
                        bInterfaceNumber = 0;
                        bAlternateSetting = 0;
                    }else if(dev_desc->idVendor == 0x483 && dev_desc->idProduct == 0x5720){
                        bInterfaceNumber = 1;
                        bAlternateSetting = 0;
                    } else if(dev_desc->idVendor == 0x416 && dev_desc->idProduct == 0x5011){
                        bInterfaceNumber = 0;
                        bAlternateSetting = 0;
                    }

                    while (usb_host_interface_claim(driver_obj.client_hdl,
                                                driver_obj.dev_hdl, bInterfaceNumber, bAlternateSetting) != ESP_OK)
                    {
                        vTaskDelay(100);
                    }
                }
			}
			if (driver_obj.actions & ACTION_GET_CONFIG_DESC) {
				action_get_config_desc(&driver_obj);
			}
			if (driver_obj.actions & ACTION_GET_STR_DESC) {
				action_get_str_desc(&driver_obj);

				transfer->device_handle = driver_obj.dev_hdl;
				transfer->bEndpointAddress = endpoint_addr;
				transfer->callback = transfer_cb;
				transfer->context = (void*) &driver_obj;

				set_setup_done(true);
				set_printer_error(false);
			}
			if (driver_obj.actions & ACTION_CLOSE_DEV) {
				action_close_dev(&driver_obj);
			}
			if (driver_obj.actions & ACTION_EXIT) {
				break;
			}

			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}

	ESP_LOGI(TAG, "Deregistering Client");
	//ESP_ERROR_CHECK(
	usb_host_client_deregister(driver_obj.client_hdl);
	//);

//	do {
//		if (usb_host_device_free_all() != ESP_ERR_NOT_FINISHED) {
//			printf("usb_host_device_free_all confirmed\n\n");
//			break;
//		}
//
//		printf("usb_host_device_free_all NOT confirmed\n\n");
//
//		vTaskDelay(500);
//	} while (1);

	set_printer_connected(false);

	//Wait to be deleted
	xSemaphoreGive(signaling_sem);
	vTaskSuspend(NULL);
}

void usb_class_driver_setup(SemaphoreHandle_t &signaling_sem,
		TaskHandle_t &class_driver_task_hdl) {

	//Create the class driver task
	xTaskCreatePinnedToCore(class_driver_task, "USB_CLASS_TASK", 4096,
			(void*) signaling_sem, CLASS_TASK_PRIORITY, &class_driver_task_hdl,
			0);
}
