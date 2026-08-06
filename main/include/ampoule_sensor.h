#ifndef MAIN_INCLUDE_AMPOULE_SENSOR_H_
#define MAIN_INCLUDE_AMPOULE_SENSOR_H_

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

/* PCF8574 i2C Address
 *
 * Address Pin
 *    A0   |   A1   |   A2   |   Addr i2c
 *	   0   |    0   |    0   |    0x20
 *	   1   |    0   |    0   |    0x21
 *	   0   |    1   |    0   |    0x22
 *	   1   |    1   |    0   |    0x23
 *	   0   |    0   |    1   |    0x24
 *	   1   |    0   |    1   |    0x25
 *	   0   |    1   |    1   |    0x26
 *	   1   |    1   |    1   |    0x27
 */

#define P0 0
#define P1 1
#define P2 2
#define P3 3
#define P4 4
#define P5 5
#define P6 6
#define P7 7

#define I2C_AMPOULES_ADDRESS 0x20


void ampoule_sensor_setup();
void ampoule_sensor_main();

bool check_if_ampoules_is_present_on_init();
bool check_if_ampoules_is_confirmed_present_in_init();

#endif
