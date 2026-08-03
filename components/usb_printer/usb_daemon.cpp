#include "usb_daemon.h"
#include "usb_utils.h"
#include "usb_class_driver.h"

static const char *TAG = "USB_DAEMON";

static void host_lib_daemon_task(void *arg) {
	SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t) arg;

	ESP_LOGI(TAG, "Installing USB Host Library");
	usb_host_config_t host_config = { .skip_phy_setup = false, .intr_flags =
	ESP_INTR_FLAG_LEVEL1, };
	ESP_ERROR_CHECK(usb_host_install(&host_config));

	//Signal to the class driver task that the host library is installed
	xSemaphoreGive(signaling_sem);
	vTaskDelay(10); //Short delay to let client task spin up

	// A lib USB host fica instalada para sempre - o equipamento nunca
	// desliga a impressora de proposito. Tentamos reinstalar a lib em
	// runtime quando a impressora desconecta (pra redetectar reconexao sem
	// reboot), mas isso se mostrou instavel em teste fisico (3 formas
	// diferentes de crash/deadlock) - a recuperacao de desconexao agora e
	// feita via esp_restart() protegido (ver printer.cpp), nao aqui.
	for (;;) {
		uint32_t event_flags;
		ESP_ERROR_CHECK(
				usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
	}
}

void usb_daemon_setup() {

	SemaphoreHandle_t signaling_sem = xSemaphoreCreateBinary();

	TaskHandle_t daemon_task_hdl;
	TaskHandle_t class_driver_task_hdl;
	//Create daemon task
	xTaskCreatePinnedToCore(host_lib_daemon_task, "daemon", 4096,
			(void*) signaling_sem,
			DAEMON_TASK_PRIORITY, &daemon_task_hdl, 0);
	//Create the class driver task
	xTaskCreatePinnedToCore(class_driver_task, "class", 4096,
			(void*) signaling_sem,
			CLASS_TASK_PRIORITY, &class_driver_task_hdl, 0);

	vTaskDelay(10);     //Add a short delay to let the tasks run

	//Wait for the tasks to complete
	for (int i = 0; i < 2; i++) {
		xSemaphoreTake(signaling_sem, portMAX_DELAY);
	}

	//Delete the tasks
	vTaskDelete(class_driver_task_hdl);
	vTaskDelete(daemon_task_hdl);
}
