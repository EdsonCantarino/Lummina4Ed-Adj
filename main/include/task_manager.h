#ifndef MAIN_INCLUDE_TASK_MANAGER_H_
#define MAIN_INCLUDE_TASK_MANAGER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/message_buffer.h>
#include <esp_system.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_event.h>

// EventGroups
extern EventGroupHandle_t task_manager_event_group;
extern EventGroupHandle_t sensors_event_group;

//extern TaskHandle_t ledext_task_handler;

extern MessageBufferHandle_t temperature_message_buffer;

extern TaskHandle_t led_panel_task_handle;
extern TaskHandle_t led_uv_task_handle;
extern TaskHandle_t blink_led_heater_task_handle;
extern TaskHandle_t blink_led_ampoules_task_handle;
extern TaskHandle_t print_ampoule_test_task_handle;

// EventGroups Bits
/* define event bits */
#define HEAT_TEMPERATURE_BIT        ( 1 << 0 )
#define HEAT_TEMPERATURE_ERROR_BIT        ( 1 << 1 )
//#define TASK_2_BIT        ( 1 << 1 ) //10
//#define TASK_3_BIT        ( 1 << 2 ) //100

// Sensors state EventGroups
#define SENSOR_TEMPERATURE_BIT   ( 1 << 0 )
#define SENSOR_DS1302_BIT        ( 1 << 1 )
#define SENSOR_LIGHT_BIT		 ( 1 << 2 )
#define SENSOR_BUZZER_BIT		 ( 1 << 3 )
#define SENSOR_HEATER_BIT		 ( 1 << 4 )

// Prototipos
void task_manager_setup();

#endif
