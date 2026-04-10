#include "include/task_manager.h"

#define TEMP_BUFFER_SIZE 10

static const char *TAG = "TASK_MANAGER";

EventGroupHandle_t task_manager_event_group;
EventGroupHandle_t sensors_event_group;


TaskHandle_t led_panel_task_handle;
TaskHandle_t led_uv_task_handle;
TaskHandle_t blink_led_heater_task_handle;
TaskHandle_t blink_led_ampoules_task_handle;
TaskHandle_t print_ampoule_test_task_handle;

MessageBufferHandle_t temperature_message_buffer;

// Prototipos
void task_manager_setup() {
	task_manager_event_group = xEventGroupCreate();
	sensors_event_group = xEventGroupCreate();

	temperature_message_buffer = xMessageBufferCreate(TEMP_BUFFER_SIZE);

}

