#ifndef USB_PRINTER_USB_DAEMON_H_
#define USB_PRINTER_USB_DAEMON_H_

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"

//void usb_daemon_setup(SemaphoreHandle_t &signaling_sem, TaskHandle_t &daemon_task_hdl);

void usb_daemon_setup();

#endif
