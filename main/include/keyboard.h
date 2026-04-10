#ifndef MAIN_INCLUDE_KEYBOARD_H_
#define MAIN_INCLUDE_KEYBOARD_H_

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

#define I2C_KEYBOARD_ADDRESS 0x24

#define P0 0
#define P1 1
#define P2 2
#define P3 3
#define P4 4
#define P5 5
#define P6 6
#define P7 7

#define BUTTON_DOWN (1)
#define BUTTON_UP (2)
#define BUTTON_HELD (3)

void keyboard_setup();
void keyboard_main();

bool get_buzzer_button_state();

void turn_on_buzzer_button();

void enable_buttons_functions();
void disable_buttons_functions();

#endif
