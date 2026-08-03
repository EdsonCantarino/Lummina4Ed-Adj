#ifndef USB_PRINTER_USB_CLASS_DRIVER_H_
#define USB_PRINTER_USB_CLASS_DRIVER_H_

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"

#define CLIENT_NUM_EVENT_MSG        5

#define ACTION_OPEN_DEV             0x01
#define ACTION_GET_DEV_INFO         0x02
#define ACTION_GET_DEV_DESC         0x04
#define ACTION_GET_CONFIG_DESC      0x08
#define ACTION_GET_STR_DESC         0x10
#define ACTION_CLOSE_DEV            0x20
#define ACTION_EXIT                 0x40

bool is_setup_done();
bool is_printer_error();
bool is_printer_connected();

void usb_class_driver_setup(SemaphoreHandle_t &signaling_sem, TaskHandle_t &class_driver_task_hdl);

esp_err_t transfer_cmd(uint8_t *data, int size);

void class_driver_task(void *arg);

#endif /* USB_PRINTER_USB_CLASS_DRIVER_H_ */
