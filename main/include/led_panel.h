#ifndef MAIN_INCLUDE_LED_PANEL_H_
#define MAIN_INCLUDE_LED_PANEL_H_

#ifdef __cplusplus
extern "C" {
#endif

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

#define I2C_PANEL_1_ADDRESS 0x21
#define I2C_PANEL_2_ADDRESS 0x23
#define I2C_PANEL_3_ADDRESS 0x25
#define I2C_PANEL_4_ADDRESS 0x27
#define I2C_PANEL_5_ADDRESS 0x20

void led_panel_setup();
void led_panel_main();

void heater_fail_leds(bool state);
void ampoules_leds(int ampoule, bool state);
void ampoules_leds_temp_error(int ampoule, bool state);
void ampoules_leds_removed_error(int ampoule);
void ampoules_leds_positived(int ampoule, bool positived);

void print_leds(bool state);

void wifi_led(bool state);

void set_led_function_active();
void set_led_function_deactive();

void ampoules_leds_locked_on_start_temp_error(bool state);
void ampoules_leds_disabled(int ampoule, bool state);

void start_stop_led_effect_test(bool start);
void set_led_function_on_off(bool state_led_panel1, bool state_led_panel2, bool state_led_panel3, bool state_led_panel4);

// Teste de lampada: percorre LED1->2->3->4 (tempo) so na cavidade 1,
// terminando com o LED1 aceso. Usado no modo CRC1 (unica cavidade
// habilitada) pra confirmar visualmente que os 4 niveis de tempo dessa
// cavidade funcionam - em Normal/ETO isso nao e necessario (Normal ja
// mostra LED1 aceso em todas as cavidades habilitadas; ETO nunca usa
// LED2/3/4).
void crc1_led_lamp_test_cavity1();

#ifdef __cplusplus
}
#endif

#endif
