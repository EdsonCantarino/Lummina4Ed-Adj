#include "include/ampoule_sensor.h"
#include "include/pcf8574_driver.h"
#include "include/task_manager.h"
#include "include/buzzer.h"
#include "include/led_panel.h"
#include "include/ampoules.h"
#include "include/heater.h"
#include "include/rtc_ds1302.h"
#include "include/keyboard.h"

#include "include/ampoule_test.h"
#include "advanced_config.h"

static const char *TAG = "AMPOULE";

TimerHandle_t ampoules_test_leds_timer;
TimerHandle_t ampoules_test_leds_temp_error_timer;

static TaskHandle_t ampoules_test_leds_task_handle = NULL;
static TaskHandle_t ampoules_test_leds_error_task_handle = NULL;

bool leds_ampoules_status = false;
bool leds_ampoules_temp_error_status = false;

bool leds_ampoules_disabled = true;

volatile bool ampoule_test_done = false;

void set_ampoule_led_test_done(bool is_done) {
	ampoule_test_done = is_done;
}

// Task dedicada para animação dos LEDs de teste das ampolas
// (timer callback não pode usar vTaskDelay)
void ampoules_test_leds_task(void *pvParameter) {
	while (1) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		for (int i = 0; i < 2; i++) {
			for (int j = 1; j < 5; j++) {
				if (!is_test_done(j)) {
					if (ampoule_get_status(j)) {
						ampoules_leds(j, leds_ampoules_status);
						leds_ampoules_disabled = false;
					} else {
						ampoules_leds(j, 1);
						leds_ampoules_disabled = true;
					}
				}
			}

			leds_ampoules_status = !leds_ampoules_status;

			vTaskDelay(pdMS_TO_TICKS(500));
		}
	}
}

void ampoules_test_leds_timer_call_back(TimerHandle_t xTimer) {
	// Apenas notifica a task — não bloqueia a timer service task
	if (ampoules_test_leds_task_handle != NULL) {
		xTaskNotifyGive(ampoules_test_leds_task_handle);
	}
}

void leds_ampolues_locked_on_start_error_alarm() {

	for (int i = 0; i < 4; i++) {

		ampoules_leds_locked_on_start_temp_error(
				leds_ampoules_temp_error_status);

		leds_ampoules_temp_error_status = !leds_ampoules_temp_error_status;

		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

void leds_error_alarm() {
	for (int i = 0; i < 4; i++) {
		if (!is_test_done(i)) {

			if (ampoule_get_status(1)) {
				ampoules_leds_temp_error(1, leds_ampoules_temp_error_status);
			} else {
				ampoules_leds_temp_error(1, 1);
			}

			if (ampoule_get_status(2)) {
				ampoules_leds_temp_error(2, leds_ampoules_temp_error_status);
			} else {
				ampoules_leds_temp_error(2, 1);
			}

			if (ampoule_get_status(3)) {
				ampoules_leds_temp_error(3, leds_ampoules_temp_error_status);
			} else {
				ampoules_leds_temp_error(3, 1);
			}

			if (ampoule_get_status(4)) {
				ampoules_leds_temp_error(4, leds_ampoules_temp_error_status);
			} else {
				ampoules_leds_temp_error(4, 1);
			}

			leds_ampoules_temp_error_status = !leds_ampoules_temp_error_status;
		}

		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

// Task dedicada para animação dos LEDs de erro de temperatura
// (timer callback não pode usar vTaskDelay)
void ampoules_test_leds_error_task(void *pvParameter) {
	while (1) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		leds_error_alarm();
	}
}

void ampoules_test_leds_error_timer_call_back(TimerHandle_t xTimer) {
	// Apenas notifica a task — não bloqueia a timer service task
	if (ampoules_test_leds_error_task_handle != NULL) {
		xTaskNotifyGive(ampoules_test_leds_error_task_handle);
	}
}

void ampoules_test_leds_timer_start() {
	if (xTimerIsTimerActive(ampoules_test_leds_timer) == pdFALSE) {
		xTimerStart(ampoules_test_leds_timer, 0);
	}
}

void ampoules_test_leds_timer_stop() {
	if (xTimerIsTimerActive(ampoules_test_leds_timer) != pdFALSE) {
		xTimerStop(ampoules_test_leds_timer, 0);
	}
}

void ampoules_test_leds_temp_error_timer_start() {
	if (xTimerIsTimerActive(ampoules_test_leds_temp_error_timer) == pdFALSE) {
		xTimerStart(ampoules_test_leds_temp_error_timer, 0);
	}
}

void ampoules_test_leds_temp_error_timer_stop() {
	if (xTimerIsTimerActive(ampoules_test_leds_temp_error_timer) != pdFALSE) {
		xTimerStop(ampoules_test_leds_temp_error_timer, 0);
	}
}

void read_ampoules_test_task(void *pvParameter) {
	bool function_enabled = false;

	vTaskDelay(pdMS_TO_TICKS(500));

	while (1) {

		bool ampoules_on_init = check_if_ampoules_is_present_on_init();

		bool is_locked = true;

		if (ampoules_on_init) {
			// Trava as funções, não pode ter ampolas nas cavidades quando inicia o equipamento

			disable_buttons_functions();

			// So soa o alarme (LED + buzzer) quando ha ampola de fato
			// confirmada presente. A trava fica ativa desde o boot ate
			// confirmar 10s sem ampola (ver ampoule_sensor.cpp), entao
			// sem essa checagem todo boot alarmava mesmo sem ampola
			// nenhuma instalada - bug reportado pelo cliente.
			if (check_if_ampoules_is_confirmed_present_in_init()) {
				leds_ampolues_locked_on_start_error_alarm();

				for (int i = 0; i < 3; i++) {
					buzzer_alarm();
				}
			}

			// Não retirar o delay para não travar o processamento.
			vTaskDelay(pdMS_TO_TICKS(250));
			continue;

		} else {
			if (!function_enabled) {
				if (check_if_heater_temperature_stabilized()) {
					enable_buttons_functions();
					// Acende o LED de 20min so nas cavidades habilitadas -
					// set_led_function_active() acendia nas 4 sem checar o
					// modo (ex: CRC1 com so a cavidade 1 ativa).
					ampoule_test_check_cavity_finalize();
				} else {
					set_led_function_deactive();
				}

				function_enabled = true;
			}

			is_locked = false;
		}

		bool is_temp_stabilized = check_if_heater_temperature_stabilized();
		float temp = get_heater_temperature();

		// Comportamento do firmware original (Lummina4Ed, sem "Adj"),
		// reintroduzido a pedido do usuario: ampola presente (qualquer
		// cavidade, mesmo desabilitada - usa a leitura bruta do sensor,
		// nao ampoule_any(), que ignora cavidade desabilitada) enquanto o
		// aquecedor ainda nao atingiu a liberacao para trabalho
		// (heater_release_temp_c) significa que o equipamento esta em
		// processo de aquecimento - reinicia. So chega aqui depois que a
		// trava de boot (10s de ausencia confirmada, ver ampoule_sensor.cpp)
		// ja liberou, entao nao entra em loop: apos o reset, se a ampola
		// continuar presente, quem assume e a trava de boot (alarme, sem
		// resetar de novo), nao este bloco.
		//
		// !is_any_testing() e essencial: sem isso, uma queda normal de
		// temperatura abaixo da liberacao NO MEIO de um teste ja em
		// andamento (ampola obviamente presente) tambem reiniciava o
		// equipamento - reproduzido em campo pelo cliente 18/08 (log
		// COM4, RESTART_ID=5 disparando aos 156s de um teste de 3600s,
		// "Esta em teste: Sim" no log logo antes do reset). A regra so
		// deve valer pra ampola presente ANTES do primeiro aquecimento
		// (nenhum teste ainda iniciado).
		if (check_if_ampoules_is_confirmed_present_in_init()
				&& temp < g_advanced_config.heater_release_temp_c
				&& !is_any_testing()) {
			ESP_LOGE(TAG,
					"RESTART_ID=5 - Ampola presente durante o aquecimento (temp=%.1f < liberacao=%.1f) - reiniciando.\n",
					temp, g_advanced_config.heater_release_temp_c);

			fflush(stdout);
			vTaskDelay(pdMS_TO_TICKS(100));
			esp_restart();
		}

		if (ampoule_any()) {

			bool cancelled_by_temp = is_any_present_cancelled_by_temp();

			if (check_if_heater_temperature_stabilized() && !cancelled_by_temp) {
				// Temperatura estabilizada (>= heater_release_temp_c): inicia teste
				ampoules_test_leds_temp_error_timer_stop();
				ampoules_test_leds_timer_start();

				ampoule_test_start();
			} else if (!is_temperature_in_range() || cancelled_by_temp) {
				// Temperatura fora do range de seguranca (55-65C) OU teste
				// cancelado por temperatura alta: alarme. Nao usa mais
				// check_if_heater_temperature_stabilized() aqui - esse limiar
				// e mais apertado (heater_release_temp_c) e disparava alarme
				// falso toda vez que a temperatura oscilava logo abaixo dele,
				// mesmo dentro do range seguro e com o teste rodando normal.
				ampoules_test_leds_temp_error_timer_start();

				for (int i = 0; i < 3; i++) {
					buzzer_alarm();
				}
			}
		}

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void ampoules_tests_main() {
	//ampoule_test_check_cavity();

	printf("\nTeste das cavidades desativado.\n\n");
}

void ampoules_tests_setup() {
	ampoules_test_leds_timer = xTimerCreate("AMP_TEST_LED_TMR",
			pdMS_TO_TICKS(2000),
			pdTRUE, (void*) 0, ampoules_test_leds_timer_call_back);

	ampoules_test_leds_temp_error_timer = xTimerCreate(
			"AMP_TEST_LED_TEMP_ERROR", pdMS_TO_TICKS(2000),
			pdTRUE, (void*) 0, ampoules_test_leds_error_timer_call_back);

	xTaskCreate(ampoules_test_leds_task, "AMP_LEDS_TST",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, &ampoules_test_leds_task_handle);

	xTaskCreate(ampoules_test_leds_error_task, "AMP_LEDS_ERR",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, &ampoules_test_leds_error_task_handle);

	xTaskCreate(read_ampoules_test_task, "READ_AMPOULES_TST",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, NULL);

}
