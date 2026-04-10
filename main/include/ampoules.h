#ifndef MAIN_INCLUDE_AMPOULES_H_
#define MAIN_INCLUDE_AMPOULES_H_

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/message_buffer.h>
#include <esp_system.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>
#include <pcf8574.h>
#include <string.h>
#include "config.h"

void ampoules_tests_setup();
void ampoules_tests_main();

void ampoules_test_leds_timer_start();
void ampoules_test_leds_timer_stop();

void set_ampoule_led_test_done(bool is_done);

#endif
