#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "include/heater.h"
#include "include/task_manager.h"
#include "include/heater_fail.h"

#include "include/keyboard.h"
#include "include/led_panel.h"
#include "include/ampoule_test.h"

#define ON 1
#define OFF 0

#define HEATER_TIMEOUT CONFIG_HEATER_FAIL_TIMEOUT
static const gpio_num_t HEATER_GPIO = (gpio_num_t) CONFIG_HEATER_GPIO;
static const float HEATER_TEMPERATURE = 37.0f;
static const float HEATER_TEMPERATURE_PRECISION = 2.0f;
//(float) CONFIG_HEATER_TEMPERATURE_PRECISION;

static const char *TAG = "HEATER";

// Semaphores
static SemaphoreHandle_t heater_timer_semaphore;

// TaskHandlers
static TaskHandle_t check_temperature_task_handle;

// TimerHandlers
static TimerHandle_t heater_alarm_timer_handler;

volatile bool is_heater_on = true;

// Prototipos
void heater_setup_timer();
esp_err_t heater_setup();
esp_err_t heater_start();
esp_err_t heater_stop();
esp_err_t check_temperature_sensor();
void check_heater_temperature_task(void *parameter);
void heater_temperature_timeout_callback(TimerHandle_t xTimer);
esp_err_t heater_alarm_start_timer();
esp_err_t heater_alarm_stop_timer();

esp_err_t check_heater_temperature_start_task();
esp_err_t check_heater_temperature_stop_task();

volatile float heater_temperature = -1.0f;
volatile float old_temp = -1.0f;
volatile float new_temp = -1.0f;

bool heater_temp_is_down = false;
bool heater_temp_is_up = false;

static bool tests_cancelled_on_temp_error = false;

float heater_min_temp = 35.0f;
float heater_max_temp = 43.0f;

bool enable_functions = true;
static bool heater_reached_target = false;

bool check_temperature_status(bool is_in_test) {
	float min_temp = get_min_temperature(is_in_test);
	float max_temp = get_max_temperature(is_in_test);
	if (heater_temperature < min_temp || heater_temperature > max_temp)
		return true;
	return false;
}

float get_target_temperature() {
	return HEATER_TEMPERATURE;
}

float get_min_temperature(bool is_in_test) {
	return 33.0f;
}

float get_max_temperature(bool is_in_test) {
	return 43.0f;
}

void set_heater_controlling(bool status) {
	is_heater_on = status;

	if (!is_heater_on) {
		heater_stop();
	}
}

float get_heater_temperature() {
	return heater_temperature;
}

bool is_temperature_in_range() {
	return heater_temperature >= get_min_temperature(false)
			&& heater_temperature <= get_max_temperature(false);
}

bool check_if_heater_temperature_stabilized() {
	if ((heater_temperature >= heater_min_temp)
			&& (heater_temperature <= heater_max_temp))
		return true;

	return false;
}

esp_err_t heater_setup() {
//	ESP_LOGI(TAG, "Configuring Heater!");
	gpio_reset_pin(HEATER_GPIO);

	gpio_set_direction(HEATER_GPIO, GPIO_MODE_OUTPUT);

	heater_setup_timer();

	heater_fail_setup();

//	ESP_LOGI(TAG, "Heater Configure done!");

	return ESP_OK;
}

void heater_setup_timer() {
	heater_alarm_timer_handler = xTimerCreate("HEATERALARM",
	HEATER_TIMEOUT / portTICK_PERIOD_MS, pdFALSE, NULL,
			heater_temperature_timeout_callback);

	heater_timer_semaphore = xSemaphoreCreateBinary();
}

esp_err_t heater_start() {
	gpio_set_level(HEATER_GPIO, ON);

	if (check_temperature_sensor() == ESP_OK) {
		ESP_ERROR_CHECK(check_heater_temperature_start_task());
		ESP_ERROR_CHECK(heater_alarm_start_timer());

		return ESP_OK;
	}

	return ESP_FAIL;
}

esp_err_t heater_stop() {
//	ESP_LOGD(TAG, "Stopping Heater");

	gpio_set_level(HEATER_GPIO, OFF);

	if (gpio_get_level(HEATER_GPIO) == OFF) {

		ESP_ERROR_CHECK(heater_alarm_stop_timer());

		return ESP_OK;
	} else {
//		ESP_LOGD(TAG, "Heater is Stopped");
	}

	return ESP_FAIL;
}

esp_err_t check_temperature_sensor() {
	EventBits_t x;

	int count = 0;

	while (count < 5) {
		x = xEventGroupGetBits(sensors_event_group);

		if (x & SENSOR_TEMPERATURE_BIT) {
			return ESP_OK;
		} else {
//			ESP_LOGE(TAG, "Waiting Temperature sensor");

			xEventGroupClearBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);
		}

		count++;

		vTaskDelay(1000 / portTICK_PERIOD_MS);

	}

	return ESP_FAIL;
}

void check_heater_temperature_task(void *parameter) {
//	ESP_LOGI(TAG, "Check Temperature Task\n");

	const TickType_t xBlockTime = pdMS_TO_TICKS(1000);

	size_t xReceivedBytes;
	float *temperature = 0;

	while (true) {
		xReceivedBytes = xMessageBufferReceive(temperature_message_buffer,
				(void* )&temperature, sizeof(float), xBlockTime);

		if (xReceivedBytes > 0) {
			heater_temperature = *temperature;

			if (heater_temperature >= HEATER_TEMPERATURE && !heater_reached_target) {
				heater_reached_target = true;
			}

			if (is_heater_on) {
				if (heater_temperature < HEATER_TEMPERATURE) {
					heater_start();
				} else {
					heater_stop();
				}
			}

			bool is_in_test = is_any_testing();
			bool is_temp_stabilized = check_if_heater_temperature_stabilized();
			bool is_in_range = is_temperature_in_range();

			// Removido: esp_restart() ao detectar temperatura fora do range durante teste.
			// O fluxo normal abaixo já aciona heater_fail_start() quando necessário,
			// evitando reboot indevido ao inserir ampola após alarme de temperatura alta.

			printf("\n");

			ESP_LOGE(TAG, "Temperatura: %.3f", heater_temperature);

			ESP_LOGE(TAG, "Funcoes Habilitadas: %s",
					enable_functions ? "Não" : "Sim");

			ESP_LOGE(TAG, "Esta em teste: %s", is_in_test ? "Sim" : "Nao");

			ESP_LOGE(TAG, "Temperatura estabilizada: %s",
					is_temp_stabilized ? "Sim" : "Nao");

			ESP_LOGE(TAG, "Temperatura no Range > 33 e < 43: %s",
					is_in_range ? "Sim" : "Nao");

			printf("\n");

			if (is_temp_stabilized) {
				//ESP_LOGE(TAG, "Aqui - Temperatura estabilizada");

				if (enable_functions) {
					// Aqui ativa as funções do teclado e mantem os leds ligados(1, 2, 3 e 4)
					enable_buttons_functions();
					set_led_function_active();

					enable_functions = false;

					if (!is_in_test) {
						start_stop_led_effect_test(false);
						ampoule_test_check_cavity_finalize();
					}
				}

				heater_fail_stop(); // para alarme de temperatura alta se estiver ativo
				xTaskNotify(blink_led_heater_task_handle, 0, eSetBits);
			} else {
				//ESP_LOGE(TAG, "Aqui - Temperatura NAO estabilizada");

				if (is_temperature_in_range()) {
					//ESP_LOGE(TAG, "Aqui - Temperatura no Range de 50 - 68");
					heater_fail_stop(); // temperatura voltou ao range, para alarme
					tests_cancelled_on_temp_error = false;
					xTaskNotify(blink_led_heater_task_handle, 0, eSetBits);
				} else {
					//ESP_LOGE(TAG, "Aqui - Fora do Range e não esta em teste");
					xTaskNotify(blink_led_heater_task_handle, 1, eSetBits);

					// Cancela todos os testes em andamento se temperatura sair do range 33-43
					if (!tests_cancelled_on_temp_error && is_any_testing()) {
						tests_cancelled_on_temp_error = true;
						trigger_temp_out_of_range_cancel();
					}

					// Alarme de temperatura fora do range:
					// - Sempre alarma se > 43 graus
					// - Só alarma se < 33 graus quando a máquina já estabilizou ao menos
					//   uma vez (!enable_functions), evitando alarme no aquecimento inicial
					if (heater_temperature > get_max_temperature(false)
							|| (!enable_functions && heater_temperature < get_min_temperature(false))) {
						heater_fail_start();
					}
				}

				ESP_LOGI(TAG, "AGUARDE: Temperatura de aquecimento atual: %.3f",
						*temperature);
			}

			//ESP_LOGE(TAG, "Esta caindo aqui fora...");

//			if (heater_temperature > HEATER_TEMPERATURE) {
//				xTaskNotify(blink_led_heater_task_handle, 0, eSetBits);
//			} else {
//				xTaskNotify(blink_led_heater_task_handle, 1, eSetBits);
//
//				ESP_LOGI(TAG, "AGUARDE: Temperatura de aquecimento atual: %.3f", *temperature);
//			}
		}

		vTaskDelay(3000 / portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}

void heater_temperature_timeout_callback(TimerHandle_t xTimer) {
	//ESP_LOGE(TAG, "Heater temperature error");

	BaseType_t xHigherPriorityTaskWoken;
	xEventGroupSetBitsFromISR(task_manager_event_group,
			HEAT_TEMPERATURE_ERROR_BIT, &xHigherPriorityTaskWoken);

	heater_stop();

	heater_fail_start();

	xSemaphoreGive(heater_timer_semaphore);
}

esp_err_t heater_alarm_start_timer() {
	if (!xTimerIsTimerActive(heater_alarm_timer_handler)) {
//		ESP_LOGD(TAG, "Starting Heater");
		xTimerStart(heater_alarm_timer_handler, portMAX_DELAY);

//		ESP_LOGD(TAG, "Successfully. Heater Temperature Alarm is ON");
	} else {
//		ESP_LOGD(TAG, "Heater is ON.");
	}

	return ESP_OK;
}

esp_err_t heater_alarm_stop_timer() {
	if (xTimerIsTimerActive(heater_alarm_timer_handler)) {
//		ESP_LOGI(TAG, "Stopping Heater timer");
		xTimerStop(heater_alarm_timer_handler, 0);
		xSemaphoreGive(heater_timer_semaphore);

//		ESP_LOGI(TAG, "Successfully. Heater Temperature Alarm is OFF");
	} else {
//		ESP_LOGI(TAG, "Heater is OFF.");
	}

	return ESP_OK;
}

esp_err_t check_heater_temperature_start_task() {
	if (check_temperature_task_handle == NULL) {

		xTaskCreate(check_heater_temperature_task, "check_heat_temp", 1024 * 5,
		NULL,
		configMAX_PRIORITIES - 5, &check_temperature_task_handle);
	}

	return ESP_OK;
}

esp_err_t check_heater_temperature_stop_task() {
	if (check_temperature_task_handle != NULL) {
		vTaskSuspend(check_temperature_task_handle);
		//vTaskDelete(check_temperature_task_handle);
	}

	return ESP_OK;
}

bool heater_has_reached_target() {
	return heater_reached_target;
}

void reset_heater_reached_target() {
	heater_reached_target = false;
}

// Retorna true após a temperatura ter atingido 37°C pela primeira vez
// e ainda estar dentro do range operacional (>= 33°C).
// Usa is_temperature_in_range() (>= 33°C) em vez de check_if_heater_temperature_stabilized()
// (>= 35°C) para que novos testes sejam aceitos assim que a temperatura voltar ao range
// após um alarme de temperatura baixa, sem exigir re-aquecimento até 35°C.
bool check_if_heater_temperature_stabilized_ampoules() {
	return heater_reached_target && is_temperature_in_range();
}
