#ifndef MAIN_INCLUDE_BUZZER_H_
#define MAIN_INCLUDE_BUZZER_H_

#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "config.h"

void buzzer_main();
esp_err_t start_buzzer();
esp_err_t stop_buzzer();

void set_buzzer_on_off();
void set_buzzer_on_off(bool status);

void buzzer_on();
void buzzer_off();

void buzzer_alarm();
void buzzer_alert(bool inverted);

void buzzer_continuous(TickType_t delay);

void set_alarm(bool status);

void set_buzzer_alert_on_off(bool status);

bool get_buzzer_on_off_status();
bool get_buzzer_alert_on_off_status();

#endif /* MAIN_INCLUDE_BUZZER_H_ */
