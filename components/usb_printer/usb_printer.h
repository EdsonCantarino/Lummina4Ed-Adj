#ifndef USB_PRINTER_H_
#define USB_PRINTER_H_

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"

void usb_printer_setup();
bool usb_print_setup_done();
void usb_print(uint8_t *data, int size, bool force_print);

#endif
