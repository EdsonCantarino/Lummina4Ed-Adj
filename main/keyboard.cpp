#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "include/pcf8574_driver.h"
#include "include/keyboard.h"
#include "include/task_manager.h"
#include "include/led_panel_pin_mapping.h"
#include "include/buzzer.h"
#include "include/ampoule_sensor.h"
#include "include/ampoule_test.h"

#include "include/nvs_utils.h"

static const gpio_num_t I2C_MASTER_SCL_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SCL;
static const gpio_num_t I2C_MASTER_SDA_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SDA;

static const char *TAG = "KEYBOARD";

PCF8574 keyboard(I2C_KEYBOARD_ADDRESS, I2C_MASTER_SDA_GPIO,
		I2C_MASTER_SCL_GPIO);

#define BUTTON_BIT(x) (1ULL<<x)

//#define BUTTON_1         247
//#define BUTTON_2         251
//#define BUTTON_3         253
//#define BUTTON_4         254
//#define BUTTON_SOUND_OFF 239
//#define BUTTON_PRINT     223
//#define BUTTON_SETTINGS  191

#define BUTTON_1         4
#define BUTTON_2         3
#define BUTTON_3         2
#define BUTTON_4         1
#define BUTTON_SOUND_OFF 5
#define BUTTON_PRINT     6
#define BUTTON_SETTINGS  7

unsigned long long button_select = (BUTTON_BIT(BUTTON_1) | BUTTON_BIT(BUTTON_2)
		| BUTTON_BIT(BUTTON_3) | BUTTON_BIT(BUTTON_4) | BUTTON_BIT(BUTTON_PRINT)
		| BUTTON_BIT(BUTTON_SOUND_OFF) | BUTTON_BIT(BUTTON_SETTINGS));

typedef struct {
	uint8_t button;
	int level = 0;
	uint16_t history;
	uint32_t down_time;
	uint32_t next_long_time;
	bool inverted;
} buttons_history_t;

typedef struct {
	uint8_t button;
	uint8_t event;
	int level;
} button_event_t;

int button_count = -1;
buttons_history_t *buttons_history;

QueueHandle_t queue;
SemaphoreHandle_t xMutex;

#define MASK   0b1111000000111111

bool is_buttons_functions_enabled = false;

void enable_buttons_functions() {
	is_buttons_functions_enabled = true;
}

void disable_buttons_functions() {
	is_buttons_functions_enabled = false;
}

bool button_rose(buttons_history_t *d) {
	if ((d->history & MASK) == 0b0000000000111111) {
		d->history = 0xffff;
		return 1;
	}
	return 0;
}

bool button_fell(buttons_history_t *d) {
	if ((d->history & MASK) == 0b1111000000000000) {
		d->history = 0x0000;
		return 1;
	}
	return 0;
}

bool button_down(buttons_history_t *d) {
	if (d->inverted)
		return button_fell(d);
	return button_rose(d);
}

bool button_up(buttons_history_t *d) {
	if (d->inverted)
		return button_rose(d);
	return button_fell(d);
}

void print_button_status(buttons_history_t *d) {
	printf("Button History Status\n");
	printf("Button: %d\n", d->button);
	printf("Level: %d\n", d->level);
	printf("History: %d\n", d->history);
	printf("Down Time: %ld\n", d->down_time);
	printf("Next Long Time: %ld\n", d->next_long_time);
	printf("Inverted: %d\n", d->inverted);

}

void update_button_history(buttons_history_t *d, int button_level) {
	d->history = (d->history << 1) | button_level;
}

void update_button_level(uint8_t button, int level) {
	for (int idx = 0; idx < button_count; idx++) {
		buttons_history_t *d = &buttons_history[idx];

		if (d->button == button) {
			d->level = level;
		}

	}
}

void turn_on_buzzer_button() {
	update_button_level(BUTTON_SOUND_OFF, 1);
	set_buzzer_on_off(1);
}

void update_button_level(buttons_history_t *d) {

	if (d->button == BUTTON_1 || d->button == BUTTON_2 || d->button == BUTTON_3
			|| d->button == BUTTON_4) {

		if (is_buttons_functions_enabled) {
			if ((d->button == BUTTON_1 && !ampoule_get_status(1)
					&& !ampoule_is_disabled(1) && !ampoule_is_locked(1))
					|| (d->button == BUTTON_2 && !ampoule_get_status(2)
							&& !ampoule_is_disabled(2) && !ampoule_is_locked(2))
					|| (d->button == BUTTON_3 && !ampoule_get_status(3)
							&& !ampoule_is_disabled(3) && !ampoule_is_locked(3))
					|| (d->button == BUTTON_4 && !ampoule_get_status(4)
							&& !ampoule_is_disabled(4) && !ampoule_is_locked(4))) {

				d->level = (d->level + 1);

				if (d->level > 4)
					d->level = 1;
			}
		} else {
			ESP_LOGI(TAG,
					"Funcoes desativadas, aguardando estabilizacao da temperatura");
		}

	} else {
		d->level = (d->level + 1);

		if (d->level > 1)
			d->level = 0;

		if (d->button == BUTTON_SOUND_OFF) {
			buzzer_on();
			vTaskDelay(pdMS_TO_TICKS(100));
			buzzer_off();

			set_buzzer_on_off();

//			if (d->level == 1) {
//				buzzer_on();
//				vTaskDelay(pdMS_TO_TICKS(100));
//				buzzer_off();
//			} else {
//				buzzer_on();
//				vTaskDelay(pdMS_TO_TICKS(100));
//				buzzer_off();
//				vTaskDelay(pdMS_TO_TICKS(50));
//				buzzer_on();
//				vTaskDelay(pdMS_TO_TICKS(100));
//				buzzer_off();
//			}
		}
	}
}

void create_button_history() {

	if (buttons_history == NULL) {
		buttons_history = (buttons_history_t*) calloc(7,
				sizeof(buttons_history_t));
	}

	button_count = 0;
	uint32_t idx = 0;
	for (int pin = 1; pin < 8; pin++) {
		ESP_LOGI(TAG, "Registering button input: %d", pin);

		buttons_history[idx].button = pin;

		if (pin == BUTTON_1 || pin == BUTTON_2 || pin == BUTTON_3
				|| pin == BUTTON_4) {
			buttons_history[idx].level = 1;
		}

		if (pin == BUTTON_SOUND_OFF || pin == BUTTON_PRINT
				|| pin == BUTTON_SETTINGS) {
			buttons_history[idx].level = 0;
		}

		buttons_history[idx].down_time = 0;
		buttons_history[idx].inverted = true;
		if (buttons_history[idx].inverted)
			buttons_history[idx].history = 0xffff;

		idx++;
	}

	button_count = idx;
}

void keyboard_setup() {
	keyboard.begin();

	keyboard.pin_mode(P0, INPUT);
	keyboard.pin_mode(P1, INPUT);
	keyboard.pin_mode(P2, INPUT);
	keyboard.pin_mode(P3, INPUT);
	keyboard.pin_mode(P4, INPUT);
	keyboard.pin_mode(P5, INPUT);
	keyboard.pin_mode(P6, INPUT);
	keyboard.pin_mode(P7, INPUT);

	create_button_history();

	queue = xQueueCreate(4, sizeof(button_event_t));
}

uint32_t millis() {
	int64_t timer = esp_timer_get_time();
	return timer / 1000;
}

void send_event(buttons_history_t db, uint8_t ev) {
	button_event_t event = { .button = db.button, .event = ev, .level = db.level
			- 1 };
	xQueueSend(queue, &event, portMAX_DELAY);
}
/*
void button_event_task(void *pvParameter) {
	button_event_t ev;

	uint8_t panel = 0;

	while (true) {
		if (xQueueReceive(queue, &ev, 1000 / portTICK_PERIOD_MS)) {

			if (ev.event == BUTTON_UP) {

				if (ev.button == BUTTON_1 && !ampoule_is_locked(1) && !ampoule_is_disabled(1)) {
					panel = LED_PANEL1;

					ampoule_set_time_test(1, ev.level);
				}
				if (ev.button == BUTTON_2 && !ampoule_is_locked(2) && !ampoule_is_disabled(2)) {
					panel = LED_PANEL2;

					ampoule_set_time_test(2, ev.level);
				}
				if (ev.button == BUTTON_3 && !ampoule_is_locked(3) && !ampoule_is_disabled(3)) {
					panel = LED_PANEL3;

					ampoule_set_time_test(3, ev.level);
				}
				if (ev.button == BUTTON_4 && !ampoule_is_locked(4) && !ampoule_is_disabled(4)) {
					panel = LED_PANEL4;

					ampoule_set_time_test(4, ev.level);
				}

				if ((ev.button == BUTTON_1 && !ampoule_is_locked(1) && !ampoule_is_disabled(1))
						|| (ev.button == BUTTON_2 && !ampoule_is_locked(2) && !ampoule_is_disabled(2))
						|| (ev.button == BUTTON_3 && !ampoule_is_locked(3) && !ampoule_is_disabled(3))
						|| (ev.button == BUTTON_4 && !ampoule_is_locked(4) && !ampoule_is_disabled(4))) {

					if (is_buttons_functions_enabled) {
						if (ev.level == 0) {
							xTaskNotify(led_panel_task_handle,
									LED_OFF(panel, 9), eSetBits);
						} else {
							xTaskNotify(led_panel_task_handle,
									LED_ON(panel, ev.level), eSetBits);
						}
					}
				}

				if (ev.button == BUTTON_PRINT) {
					xTaskNotify(print_ampoule_test_task_handle, 1, eSetBits);
				}

				if (ev.button == BUTTON_SETTINGS) {
//					buzzer_continuous(pdMS_TO_TICKS(250));
//					esp_err_t err = sync_device_settings();
//
//					if(err != ESP_OK){
//						ESP_LOGE(TAG, "Error (%s) to sync device settings!\n", esp_err_to_name(err));
//					}

					ESP_LOGE(TAG,
							"Este botao nao possue nenhuma funcionalidade!\n");
				}
			}
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}
*/

void button_event_task(void *pvParameter) {
	button_event_t ev;

	uint8_t panel = 0;

	while (true) {
		if (xQueueReceive(queue, &ev, 1000 / portTICK_PERIOD_MS)) {

			if (ev.event == BUTTON_UP) {

				if (ev.button == BUTTON_1 && !ampoule_is_locked(1) && !ampoule_is_disabled(1)) {
					panel = LED_PANEL1;
				}
				if (ev.button == BUTTON_2 && !ampoule_is_locked(2) && !ampoule_is_disabled(2)) {
					panel = LED_PANEL2;
				}
				if (ev.button == BUTTON_3 && !ampoule_is_locked(3) && !ampoule_is_disabled(3)) {
					panel = LED_PANEL3;
				}
				if (ev.button == BUTTON_4 && !ampoule_is_locked(4) && !ampoule_is_disabled(4)) {
					panel = LED_PANEL4;
				}

				if ((ev.button == BUTTON_1 && !ampoule_is_locked(1) && !ampoule_is_disabled(1))
						|| (ev.button == BUTTON_2 && !ampoule_is_locked(2) && !ampoule_is_disabled(2))
						|| (ev.button == BUTTON_3 && !ampoule_is_locked(3) && !ampoule_is_disabled(3))
						|| (ev.button == BUTTON_4 && !ampoule_is_locked(4) && !ampoule_is_disabled(4))) {

					/* Lummina4Ed 37:
					 * botoes de tempo desabilitados.
					 * Nao altera tempo e nao altera LEDs de nivel.
					 */
				}

				if (ev.button == BUTTON_PRINT) {
					xTaskNotify(print_ampoule_test_task_handle, 1, eSetBits);
				}

				if (ev.button == BUTTON_SETTINGS) {
//					buzzer_continuous(pdMS_TO_TICKS(250));
//					esp_err_t err = sync_device_settings();
//
//					if(err != ESP_OK){
//						ESP_LOGE(TAG, "Error (%s) to sync device settings!\n", esp_err_to_name(err));
//					}

					ESP_LOGE(TAG,
							"Este botao nao possue nenhuma funcionalidade!\n");
				}
			}
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

void button_task(void *pvParameter) {

	while (1) {

		for (int idx = 0; idx < button_count; idx++) {
			uint8_t value = keyboard.digital_read(idx);

			if (value == 1) {
				update_button_history(&buttons_history[idx], 0);
				continue;
			}

			update_button_history(&buttons_history[idx], 1);

			if (button_up(&buttons_history[idx])) {
				buttons_history[idx].down_time = 0;
				ESP_LOGI(TAG, "%d UP", buttons_history[idx].button);

				update_button_level(&buttons_history[idx]);

				send_event(buttons_history[idx], BUTTON_UP);

			} else if (buttons_history[idx].down_time
					&& millis() >= buttons_history[idx].next_long_time) {
				ESP_LOGI(TAG, "%d LONG", buttons_history[idx].button);
				buttons_history[idx].next_long_time =
						buttons_history[idx].next_long_time
								+ CONFIG_ESP32_BUTTON_LONG_PRESS_REPEAT_MS;
				//send_event(debounce[idx], BUTTON_HELD);
			} else if (button_down(&buttons_history[idx])
					&& buttons_history[idx].down_time == 0) {
				buttons_history[idx].down_time = millis();
				ESP_LOGI(TAG, "%d DOWN", buttons_history[idx].button);
				buttons_history[idx].next_long_time =
						buttons_history[idx].down_time
								+ CONFIG_ESP32_BUTTON_LONG_PRESS_DURATION_MS;
				//send_event(debounce[idx], BUTTON_DOWN);
			}
		}

		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}

void keyboard_main() {
//xMutex = xSemaphoreCreateMutex();

	xTaskCreate(button_event_task, "button_event", configMINIMAL_STACK_SIZE * 5,
	NULL, 6, NULL);

	xTaskCreate(button_task, "button_task", configMINIMAL_STACK_SIZE * 5,
	NULL, 5, NULL);
}

bool get_buzzer_button_state() {
	bool state = false;

	for (int idx = 0; idx < button_count; idx++) {
		if (buttons_history[idx].button == BUTTON_SOUND_OFF) {
			state = buttons_history[idx].level;
			break;
		}
	}

	return state;
}

