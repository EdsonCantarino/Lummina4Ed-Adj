#ifndef MAIN_INCLUDE_HEATER_FAIL_H_
#define MAIN_INCLUDE_HEATER_FAIL_H_

#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "config.h"

void heater_fail_setup();
void heater_fail_start();
void heater_fail_stop();

#endif
