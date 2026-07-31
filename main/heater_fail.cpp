#include "include/heater_fail.h"
#include "include/led_panel.h"
#include "include/led_panel_pin_mapping.h"
#include "include/task_manager.h"
#include "include/buzzer.h"
#include "include/keyboard.h"

TimerHandle_t heater_fail_timer;
static TaskHandle_t heater_fail_task_handle = NULL;

void heater_fail_task(void *pvParameter) {
	while (1) {
		// Aguarda notificação do timer — não bloqueia a timer service task
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		bool alarm_off = get_buzzer_button_state();

		printf("[DEBUG-ALARM] heater_fail_task disparou (ciclo de 10s) - alarm_off=%d\n",
				alarm_off);

		if (!alarm_off) {
			bool state = false;

			for (int i = 0; i < 11; i++) {
				heater_fail_leds(state);

				state = !state;

				vTaskDelay(pdMS_TO_TICKS(250));
			}

			for (int i = 0; i < 10; i++) {
				buzzer_on();
				vTaskDelay(pdMS_TO_TICKS(150));
				buzzer_off();
				vTaskDelay(pdMS_TO_TICKS(50));
			}
		}

		heater_fail_leds(true); // true = 1 = LED_OFF (active-low): apaga os LEDs
	}
}

void heater_fail_timer_call_back(TimerHandle_t xTimer) {
	// Apenas notifica a task — nunca bloqueia aqui
	if (heater_fail_task_handle != NULL) {
		xTaskNotifyGive(heater_fail_task_handle);
	}
}

void heater_fail_setup() {
	heater_fail_timer = xTimerCreate("HEATER_FAIL_TIMER", pdMS_TO_TICKS(10000),
	pdTRUE, (void*) 0, heater_fail_timer_call_back);

	xTaskCreate(heater_fail_task, "HEATER_FAIL", 1024 * 5, NULL,
	configMAX_PRIORITIES - 5, &heater_fail_task_handle);
}

void heater_fail_start() {
	if (xTimerIsTimerActive(heater_fail_timer) == pdFALSE) {
		printf("[DEBUG-ALARM] heater_fail_start(): iniciando timer de alarme (estava parado)\n");
		xTimerStart(heater_fail_timer, 0);
	}
}

void heater_fail_stop() {
	if (xTimerIsTimerActive(heater_fail_timer) != pdFALSE) {
		xTimerStop(heater_fail_timer, 0);
	}
}
