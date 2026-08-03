#include "include/pcf8574_driver.h"
#include "include/led_panel.h"
#include "include/led_panel_pin_mapping.h"
#include "include/task_manager.h"
#include "include/buzzer.h"

#define LED_ON 0
#define LED_OFF 1

static const gpio_num_t I2C_MASTER_SCL_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SCL;
static const gpio_num_t I2C_MASTER_SDA_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SDA;

PCF8574 led_panel_1(I2C_PANEL_1_ADDRESS, I2C_MASTER_SDA_GPIO,
		I2C_MASTER_SCL_GPIO);
PCF8574 led_panel_2(I2C_PANEL_2_ADDRESS, I2C_MASTER_SDA_GPIO,
		I2C_MASTER_SCL_GPIO);
PCF8574 led_panel_3(I2C_PANEL_3_ADDRESS, I2C_MASTER_SDA_GPIO,
		I2C_MASTER_SCL_GPIO);
PCF8574 led_panel_4(I2C_PANEL_4_ADDRESS, I2C_MASTER_SDA_GPIO,
		I2C_MASTER_SCL_GPIO);

TaskHandle_t task_handle_ledext;
TaskHandle_t task_efect_led_handle;

void change_led_status(PCF8574 *led_panel, uint8_t pin, int state) {
	configASSERT(led_panel != NULL);

	led_panel->digital_write(pin, state);
}

void set_all(PCF8574 *led_panel, int state) {
	for (int i = 0; i < 8; i++) {
		led_panel->digital_write(i, state);
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void clear_panel(PCF8574 *led_panel) {
	for (int i = 0; i < 8; i++) {
		led_panel->digital_write(i, 1);
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	led_panel->digital_write(P0, 0);
}

void clear_time_panel(PCF8574 *led_panel) {
	for (int i = 0; i < 4; i++) {
		led_panel->digital_write(i, 1);
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void clear_all() {
	set_all(&led_panel_1, 1);
	set_all(&led_panel_2, 1);
	set_all(&led_panel_3, 1);
	set_all(&led_panel_4, 1);
}

void led_panel_setup() {
	led_panel_1.begin();
	led_panel_2.begin();
	led_panel_3.begin();
	led_panel_4.begin();

	led_panel_1.pin_mode(P0, OUTPUT);
	led_panel_1.pin_mode(P1, OUTPUT);
	led_panel_1.pin_mode(P2, OUTPUT);
	led_panel_1.pin_mode(P3, OUTPUT);
	led_panel_1.pin_mode(P4, OUTPUT);
	led_panel_1.pin_mode(P5, OUTPUT);
	led_panel_1.pin_mode(P6, OUTPUT);
	led_panel_1.pin_mode(P7, OUTPUT);

	led_panel_2.pin_mode(P0, OUTPUT);
	led_panel_2.pin_mode(P1, OUTPUT);
	led_panel_2.pin_mode(P2, OUTPUT);
	led_panel_2.pin_mode(P3, OUTPUT);
	led_panel_2.pin_mode(P4, OUTPUT);
	led_panel_2.pin_mode(P5, OUTPUT);
	led_panel_2.pin_mode(P6, OUTPUT);
	led_panel_2.pin_mode(P7, OUTPUT);

	led_panel_3.pin_mode(P0, OUTPUT);
	led_panel_3.pin_mode(P1, OUTPUT);
	led_panel_3.pin_mode(P2, OUTPUT);
	led_panel_3.pin_mode(P3, OUTPUT);
	led_panel_3.pin_mode(P4, OUTPUT);
	led_panel_3.pin_mode(P5, OUTPUT);
	led_panel_3.pin_mode(P6, OUTPUT);
	led_panel_3.pin_mode(P7, OUTPUT);

	led_panel_4.pin_mode(P0, OUTPUT);
	led_panel_4.pin_mode(P1, OUTPUT);
	led_panel_4.pin_mode(P2, OUTPUT);
	led_panel_4.pin_mode(P3, OUTPUT);
	led_panel_4.pin_mode(P4, OUTPUT);
	led_panel_4.pin_mode(P5, OUTPUT);
	led_panel_4.pin_mode(P6, OUTPUT);
	led_panel_4.pin_mode(P7, OUTPUT);

	clear_all();
}

PCF8574* get_panel_led(uint8_t value) {
	uint8_t v = value;

	if (value >= 128)
		v = value - 128;

	if (v >= 10 && v < 20)
		return &led_panel_1;

	if (v >= 20 && v < 30)
		return &led_panel_2;

	if (v >= 30 && v < 40)
		return &led_panel_3;

	//if (v >= 40 && v < 50)
	return &led_panel_4;
}

uint8_t get_led_pin(uint8_t value) {
	uint8_t v = value;

	if (value >= 128)
		v = value - 128;

	v = (v % 10);

	printf("LED Pin: %d\n", v);

	return v;
}

int get_led_status(uint8_t value) {
	if (value >= 128)
		return 1;
	else
		return 0;
}

void led_panel_task_notify(void *pvParameter) {
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(500);
	BaseType_t xResult;

	uint32_t ulNotifiedValue;

	for (;;) {
		/* Wait to be notified of an interrupt. */
		xResult = xTaskNotifyWait(pdFALSE, /* Don't clear bits on entry. */
		ULONG_MAX, /* Clear all bits on exit. */
		&ulNotifiedValue, /* Stores the notified value. */
		xMaxBlockTime);

		if (xResult == pdPASS) {
			//printf("Valor recebido da notifica��o: %d\n", ulNotifiedValue);

			PCF8574 *lp = get_panel_led(ulNotifiedValue);
			uint8_t led_pin = get_led_pin(ulNotifiedValue);
			bool led_status = get_led_status(ulNotifiedValue);

			if (led_pin <= 7) {
				if (led_pin < 4) {
					clear_time_panel(lp);
				}

				change_led_status(lp, led_pin, led_status);
			} else {
				clear_panel(lp);
			}
		}

		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void blink_led_heater(void *pvParameter) {
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(500);
	BaseType_t xResult;

	uint32_t ulNotifiedValue;
	bool historyState = 0;

	while (1) {
		xResult = xTaskNotifyWait(pdFALSE, /* Don't clear bits on entry. */
		ULONG_MAX, /* Clear all bits on exit. */
		&ulNotifiedValue, /* Stores the notified value. */
		xMaxBlockTime);

		if (xResult == pdPASS) {
//			printf("**** Valor recebido da notifica��o do Blink: %d\n",
//					ulNotifiedValue);

			if (ulNotifiedValue != historyState)
				historyState = ulNotifiedValue;
		}

		if (historyState == 1) {
			led_panel_2.digital_write(P6, 0);
			vTaskDelay(pdMS_TO_TICKS(1000));
			led_panel_2.digital_write(P6, 1);
		}

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void heater_fail_leds(bool state) {
	change_led_status(&led_panel_1, P4, state);
	change_led_status(&led_panel_1, P5, state);

	change_led_status(&led_panel_2, P4, state);
	change_led_status(&led_panel_2, P5, state);

	change_led_status(&led_panel_3, P4, state);
	change_led_status(&led_panel_3, P5, state);

	change_led_status(&led_panel_4, P4, state);
	change_led_status(&led_panel_4, P5, state);
}

void ampoules_leds(int ampoule, bool state) {
	if (ampoule == 1) {
		change_led_status(&led_panel_1, P4, state);
		change_led_status(&led_panel_1, P5, state);
	}

	if (ampoule == 2) {
		change_led_status(&led_panel_2, P4, state);
		change_led_status(&led_panel_2, P5, state);
	}

	if (ampoule == 3) {
		change_led_status(&led_panel_3, P4, state);
		change_led_status(&led_panel_3, P5, state);
	}

	if (ampoule == 4) {
		change_led_status(&led_panel_4, P4, state);
		change_led_status(&led_panel_4, P5, state);
	}
}

void ampoules_leds_locked_on_start_temp_error(bool state) {

	change_led_status(&led_panel_1, P0, state);
	change_led_status(&led_panel_1, P4, state);
	change_led_status(&led_panel_1, P5, state);

	change_led_status(&led_panel_2, P0, state);
	change_led_status(&led_panel_2, P4, state);
	change_led_status(&led_panel_2, P5, state);

	change_led_status(&led_panel_3, P0, state);
	change_led_status(&led_panel_3, P4, state);
	change_led_status(&led_panel_3, P5, state);

	change_led_status(&led_panel_4, P0, state);
	change_led_status(&led_panel_4, P4, state);
	change_led_status(&led_panel_4, P5, state);

}

void ampoules_leds_temp_error(int ampoule, bool state) {
	if (ampoule == 1) {
		change_led_status(&led_panel_1, P4, state);
	}

	if (ampoule == 2) {
		change_led_status(&led_panel_2, P4, state);
	}

	if (ampoule == 3) {
		change_led_status(&led_panel_3, P4, state);
	}

	if (ampoule == 4) {
		change_led_status(&led_panel_4, P4, state);
	}
}

void print_leds(bool state) {
	change_led_status(&led_panel_1, P6, state);
}

void wifi_led(bool state) {
	change_led_status(&led_panel_1, P7, state);
}

void ampoules_leds_positived(int ampoule, bool positived) {
	if (positived) {
		if (ampoule == 1) {
			change_led_status(&led_panel_1, P4, 0);
			change_led_status(&led_panel_1, P5, 1);
		}

		if (ampoule == 2) {
			change_led_status(&led_panel_2, P4, 0);
			change_led_status(&led_panel_2, P5, 1);
		}

		if (ampoule == 3) {
			change_led_status(&led_panel_3, P4, 0);
			change_led_status(&led_panel_3, P5, 1);
		}

		if (ampoule == 4) {
			change_led_status(&led_panel_4, P4, 0);
			change_led_status(&led_panel_4, P5, 1);
		}
	} else {
		if (ampoule == 1) {
			change_led_status(&led_panel_1, P4, 1);
			change_led_status(&led_panel_1, P5, 0);
		}

		if (ampoule == 2) {
			change_led_status(&led_panel_2, P4, 1);
			change_led_status(&led_panel_2, P5, 0);
		}

		if (ampoule == 3) {
			change_led_status(&led_panel_3, P4, 1);
			change_led_status(&led_panel_3, P5, 0);
		}

		if (ampoule == 4) {
			change_led_status(&led_panel_4, P4, 1);
			change_led_status(&led_panel_4, P5, 0);
		}
	}
}

void ampoules_leds_removed_error(int ampoule) {
	bool state = false;

	if (ampoule == 1) {
		change_led_status(&led_panel_1, P4, state);
	}

	if (ampoule == 2) {
		change_led_status(&led_panel_2, P4, state);
	}

	if (ampoule == 3) {
		change_led_status(&led_panel_3, P4, state);
	}

	if (ampoule == 4) {
		change_led_status(&led_panel_4, P4, state);
	}
}

void set_led_function_active() {
	led_panel_1.digital_write(P0, 0);
	led_panel_2.digital_write(P0, 0);
	led_panel_3.digital_write(P0, 0);
	led_panel_4.digital_write(P0, 0);
}

void crc1_led_lamp_test_cavity1() {
	for (int p = 0; p < 4; p++) {
		led_panel_1.digital_write(p, 0); // ON (ativo em nivel baixo)
		vTaskDelay(pdMS_TO_TICKS(300));
		led_panel_1.digital_write(p, 1); // OFF
	}

	// Volta ao padrao (LED1 aceso), mesma convencao usada em Normal/ETO.
	led_panel_1.digital_write(P0, 0);
}

void set_led_function_on_off(bool state_led_panel1, bool state_led_panel2,
		bool state_led_panel3, bool state_led_panel4) {
	led_panel_1.digital_write(P0, state_led_panel1);
	led_panel_2.digital_write(P0, state_led_panel2);
	led_panel_3.digital_write(P0, state_led_panel3);
	led_panel_4.digital_write(P0, state_led_panel4);
}

void set_led_function_deactive() {
	led_panel_1.digital_write(P0, 1);
	led_panel_2.digital_write(P0, 1);
	led_panel_3.digital_write(P0, 1);
	led_panel_4.digital_write(P0, 1);
}

void efeito_giroflex(int led, uint8_t pin, int state) {

	if (led == 0) {
		change_led_status(&led_panel_1, pin, state);
	} else if (led == 1) {
		change_led_status(&led_panel_2, pin, state);
	} else if (led == 2) {
		change_led_status(&led_panel_3, pin, state);
	} else {
		change_led_status(&led_panel_4, pin, state);
	}
}

void efeito_giroflex() {
	const int total_leds = 2;
	const int leds[total_leds] = { 4, 5 };

	for (int i = 0; i < 4; i++) {

		for (int p = 0; p < total_leds; p++) {

			if (p == 0) {
				efeito_giroflex(i, leds[total_leds - 1], LED_ON);
			} else {
				efeito_giroflex(i, p - 1, LED_ON);
			}

			efeito_giroflex(i, leds[p], LED_ON);

			vTaskDelay(20);

			if (p == 0) {
				efeito_giroflex(i, leds[total_leds - 1], LED_OFF);
			} else {
				efeito_giroflex(i, p - 1, LED_OFF);
			}

			efeito_giroflex(i, leds[p], LED_OFF);
		}

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void efeito_giroflex_2() {
	const int total_leds = 2;
	const int leds[total_leds] = { 4, 5 };

	for (int i = 0; i < 4; i++) {

		for (int p = 0; p < total_leds; p++) {

			if (p == 0) {
				change_led_status(&led_panel_1, leds[total_leds - 1], LED_ON);
				change_led_status(&led_panel_2, leds[total_leds - 1], LED_ON);
				change_led_status(&led_panel_3, leds[total_leds - 1], LED_ON);
				change_led_status(&led_panel_4, leds[total_leds - 1], LED_ON);
			} else {
				change_led_status(&led_panel_1, p - 1, LED_ON);
				change_led_status(&led_panel_2, p - 1, LED_ON);
				change_led_status(&led_panel_3, p - 1, LED_ON);
				change_led_status(&led_panel_4, p - 1, LED_ON);
			}

			change_led_status(&led_panel_1, leds[p], LED_ON);
			change_led_status(&led_panel_2, leds[p], LED_ON);
			change_led_status(&led_panel_3, leds[p], LED_ON);
			change_led_status(&led_panel_4, leds[p], LED_ON);

			vTaskDelay(20);

			if (p == 0) {
				change_led_status(&led_panel_1, leds[total_leds - 1], LED_OFF);
				change_led_status(&led_panel_2, leds[total_leds - 1], LED_OFF);
				change_led_status(&led_panel_3, leds[total_leds - 1], LED_OFF);
				change_led_status(&led_panel_4, leds[total_leds - 1], LED_OFF);
			} else {
				change_led_status(&led_panel_1, p - 1, LED_OFF);
				change_led_status(&led_panel_2, p - 1, LED_OFF);
				change_led_status(&led_panel_3, p - 1, LED_OFF);
				change_led_status(&led_panel_4, p - 1, LED_OFF);
			}

			change_led_status(&led_panel_1, leds[p], LED_OFF);
			change_led_status(&led_panel_2, leds[p], LED_OFF);
			change_led_status(&led_panel_3, leds[p], LED_OFF);
			change_led_status(&led_panel_4, leds[p], LED_OFF);
		}

		vTaskDelay(pdMS_TO_TICKS(500));

	}
}

void blink_led_test_cavities(void *pvParameter) {
	while (true) {

		efeito_giroflex();
		//efeito_giroflex_2();
		//efeito_pares_impares();

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void start_stop_led_effect_test(bool start) {
	if (blink_led_heater_task_handle != NULL) {
		if (start) {
			vTaskResume(task_efect_led_handle);
		} else {
			vTaskSuspend(task_efect_led_handle);

			// Apaga apenas os LEDs animados pelo efeito (P4 e P5)
			change_led_status(&led_panel_1, P4, LED_OFF);
			change_led_status(&led_panel_1, P5, LED_OFF);
			change_led_status(&led_panel_2, P4, LED_OFF);
			change_led_status(&led_panel_2, P5, LED_OFF);
			change_led_status(&led_panel_3, P4, LED_OFF);
			change_led_status(&led_panel_3, P5, LED_OFF);
			change_led_status(&led_panel_4, P4, LED_OFF);
			change_led_status(&led_panel_4, P5, LED_OFF);
		}
	}
}

void led_panel_main() {
	//BaseType_t xReturned;

	xTaskCreate(led_panel_task_notify, "LED_PANEL",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, &led_panel_task_handle);

	xTaskCreate(blink_led_heater, "BLINK_HEATER", configMINIMAL_STACK_SIZE * 5,
	NULL, 6, &blink_led_heater_task_handle);

	xTaskCreate(blink_led_test_cavities, "BLINK_TESTS",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 7, &task_efect_led_handle);

//	if (xReturned == pdPASS) {
//		vTaskSuspend(task_efect_led_handle);
//	}

	// LED P0 = 20 minutos fixo em todos os paineis
	led_panel_1.digital_write(P0, 0);
	led_panel_2.digital_write(P0, 0);
	led_panel_3.digital_write(P0, 0);
	led_panel_4.digital_write(P0, 0);
}

void ampoules_leds_disabled(int ampoule, bool state) {
	if (ampoule == 0) {
		change_led_status(&led_panel_1, P0, state);
		change_led_status(&led_panel_1, P1, state);
		change_led_status(&led_panel_1, P2, state);
		change_led_status(&led_panel_1, P3, state);
		change_led_status(&led_panel_1, P4, state);
		change_led_status(&led_panel_1, P5, state);
	} else if (ampoule == 1) {
		change_led_status(&led_panel_2, P0, state);
		change_led_status(&led_panel_2, P1, state);
		change_led_status(&led_panel_2, P2, state);
		change_led_status(&led_panel_2, P3, state);
		change_led_status(&led_panel_2, P4, state);
		change_led_status(&led_panel_2, P5, state);
	} else if (ampoule == 2) {
		change_led_status(&led_panel_3, P0, state);
		change_led_status(&led_panel_3, P1, state);
		change_led_status(&led_panel_3, P2, state);
		change_led_status(&led_panel_3, P3, state);
		change_led_status(&led_panel_3, P4, state);
		change_led_status(&led_panel_3, P5, state);
	} else {
		change_led_status(&led_panel_4, P0, state);
		change_led_status(&led_panel_4, P1, state);
		change_led_status(&led_panel_4, P2, state);
		change_led_status(&led_panel_4, P3, state);
		change_led_status(&led_panel_4, P4, state);
		change_led_status(&led_panel_4, P5, state);
	}
}

