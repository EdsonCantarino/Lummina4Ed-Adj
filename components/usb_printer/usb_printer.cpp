#include <stdio.h>
#include "usb_printer.h"
#include "usb_daemon.h"
#include "usb_class_driver.h"

SemaphoreHandle_t signaling_sem;
TaskHandle_t daemon_task_hdl;
TaskHandle_t class_driver_task_hdl;

void usb_printer_setup() {

//	signaling_sem = xSemaphoreCreateBinary();
//
//	usb_daemon_setup(signaling_sem, daemon_task_hdl);
//	usb_class_driver_setup(signaling_sem, class_driver_task_hdl);
//
//	vTaskDelay(10);     //Add a short delay to let the tasks run
//
//	//Wait for the tasks to complete
//	for (int i = 0; i < 2; i++) {
//		xSemaphoreTake(signaling_sem, portMAX_DELAY);
//	}
//
//	//Delete the tasks
//	vTaskDelete(class_driver_task_hdl);
//	vTaskDelete(daemon_task_hdl);
//
//	usb_printer_setup();

	usb_daemon_setup();
}

void usb_print(uint8_t *data, int size, bool force_print) {
	esp_err_t err = transfer_cmd(data, size);

	if (err != ESP_OK && force_print) {
		err = transfer_cmd(data, size);
	}
}

bool usb_print_setup_done() {
	return is_setup_done();
}
