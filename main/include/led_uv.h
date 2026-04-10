#ifndef MAIN_INCLUDE_LED_UV_H_
#define MAIN_INCLUDE_LED_UV_H_

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

#define I2C_LED_UV_ADDRESS 0x20

void led_uv_setup();
void led_uv_main();

void led_uv_on(int led);
void led_uv_off(int led);

#endif
