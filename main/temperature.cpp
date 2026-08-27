#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <cmath>

#include "include/temperature.h"
#include "include/task_manager.h"
#include "include/heater.h"
#include "LinkedList.h"
#include "cJSON.h"
#include "include/nvs_utils.h"

static const gpio_num_t SENSOR_GPIO = (gpio_num_t) CONFIG_DS18X20_ONEWIRE_GPIO;
static const int MAX_SENSORS = CONFIG_DS18X20_MAX_SENSORS;
// + ~100ms de conversao 9-bit (ds18x20.c) = leitura nova a cada ~1s.
// Reduzido de 5000ms - achado em 27/08 (comparacao video do LED do
// aquecedor x log serial) que esse delay artificial, somado ao rescan a
// cada 8 leituras que existia antes, deixava a temperatura desatualizada
// por ate ~7s, tempo suficiente pro aquecedor ficar desligado sem
// necessidade entre leituras de ADC das cavidades.
static const uint32_t LOOP_DELAY_MS = 900;

static const char *TAG = "TEMPERATURE";

// TaskHandlers
static TaskHandle_t check_temperature_task_handle;

// Prototipos
void check_temperature_task(void *pvParameter);
esp_err_t check_temperature_start_task();
esp_err_t check_temperature_stop_task();

volatile float last_temperature = 0.0f;

bool locked_list = false;

float factor_temp = 0.0f;

void temperature_setup() {
	ESP_ERROR_CHECK(check_temperature_start_task());
}

double roundTemperature(double temperature) {
//	double decimalPart = temperature - std::floor(temperature);
//
//	if (decimalPart >= 0.6 && decimalPart <= 0.9) {
//		return std::ceil(temperature);
//	} else if (decimalPart >= 0.1 && decimalPart <= 0.4) {
//		return std::floor(temperature);
//	} else {
//		return std::round(temperature * 10) / 10;
//	}

	return std::round(temperature * 2) / 2;
}

void refresh_calibration_factor(){
	get_calibration_factor(factor_temp);
}

void check_temperature_task(void *pvParameter) {
	ds18x20_addr_t addrs[MAX_SENSORS];
	float temps[MAX_SENSORS];
	size_t sensor_count = 0;

	gpio_set_pull_mode(SENSOR_GPIO, GPIO_PULLUP_ONLY);

	esp_err_t res;

	int count = 0;

	while (1) {
		// Rescan reativo: so re-escaneia o barramento se ainda nao
		// conhecemos os enderecos (primeira volta) ou se uma leitura
		// falhou de verdade (sensor_count zerado abaixo). Antes disso
		// re-escaneava a cada 8 leituras (RESCAN_INTERVAL) mesmo sem
		// motivo - o endereco ROM de um sensor fisico fixo nao muda
		// sozinho, entao isso so custava tempo sem detectar nada que uma
		// falha de leitura ja nao detectasse (achado em 27/08).
		if (sensor_count == 0) {
			res = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS,
					&sensor_count);
			if (res != ESP_OK) {
				ESP_LOGE(TAG, "Sensors scan error %d (%s)", res,
						esp_err_to_name(res));

				xEventGroupClearBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);
				vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
				continue;
			}

			if (!sensor_count) {
				ESP_LOGW(TAG, "No sensors detected!");

				xEventGroupClearBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);
				// Sem isso o loop gira sem ceder CPU quando nao acha sensor,
				// disparando o task_wdt a cada 5s (achado em 10/08 - ver
				// historico/2026-08-10-...psram-descartada.md).
				vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
				continue;
			}

			ESP_LOGD(TAG, "%d sensors detected", sensor_count);
			xEventGroupSetBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);

			// If there were more sensors found than we have space to handle,
			// just report the first MAX_SENSORS..
			if (sensor_count > MAX_SENSORS)
				sensor_count = MAX_SENSORS;

			// Configura resolucao de 9 bits (0,5 grau, ~93,75ms de
			// conversao) em cada sensor achado - roundTemperature() ja
			// arredonda pra 0,5 grau mesmo, entao os 12 bits de fabrica
			// (750ms) so custavam tempo sem ganhar precisao usada de
			// verdade (achado em 27/08).
			for (size_t i = 0; i < sensor_count; i++) {
				uint8_t scratchpad[3] = { 0x00, 0x00, TEMP_9_BIT };
				ds18x20_write_scratchpad(SENSOR_GPIO, addrs[i], scratchpad);

				// Confirma que o sensor realmente aceitou 9 bits, em vez
				// de assumir - se o byte de config lido de volta nao for
				// 0x1F, a conversao ainda esta rodando em 12 bits (750ms)
				// e o codigo so espera 100ms, lendo o valor da conversao
				// ANTERIOR em vez da atual (suspeita levantada em 27/08
				// comparando com a pistola/termometro fisico).
				uint8_t readback[8];
				esp_err_t rres = ds18x20_read_scratchpad(SENSOR_GPIO,
						addrs[i], readback);
				ESP_LOGW(TAG,
						"Sensor %d: config byte apos write = 0x%02X (esperado 0x1F) res=%d",
						(int) i, readback[4], rres);
			}
		}

		if (count == 0) {
			get_calibration_factor(factor_temp);
		}

		count++;

		if (count > 60) {
			count = 0;
		}

		res = ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count,
				temps);
		if (res != ESP_OK) {
			ESP_LOGE(TAG, "Sensors read error %d (%s)", res,
					esp_err_to_name(res));
			// Leitura falhou - sensor pode ter sido desconectado. Zera
			// sensor_count pra forcar um rescan na proxima volta, em vez
			// de continuar tentando ler um endereco que pode nao existir
			// mais.
			sensor_count = 0;
			vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
			continue;
		}

		for (int j = 0; j < sensor_count; j++) {
			float temp_c = temps[j];

			float temp_factor = roundTemperature(temp_c)
					+ roundTemperature(factor_temp);

			last_temperature = temp_factor;

			// Chamada direta em vez de mandar por message buffer pra uma
			// task consumidora separada - so existia um consumidor
			// (heater.cpp) mesmo, e a task antiga tinha um vTaskDelay(3000)
			// fixo no fim do loop, que ficou dessincronizado do produtor
			// depois que a leitura do sensor ficou mais rapida (achado em
			// 27/08 - era a causa real da cadencia de ~3s medida, nao
			// disputa de prioridade). Fundido em 27/08.
			process_heater_temperature(temp_factor);
		}

		// + ~100ms de conversao 9-bit (ds18x20.c) = leitura nova a cada ~1s.
		vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
	}
}

string read_temperatures_calibration_json() {
	cJSON *root;
	root = cJSON_CreateObject();

	//cJSON *array;
	//cJSON *element;

	while (last_temperature == 0.0f) {
		printf("Aguardando leituras do sensor /n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	cJSON *temp_result;
	temp_result = cJSON_CreateArray();

//	for (int i = 0; i < temperatures_list.size(); i++) {
//		cJSON *temp = NULL;
//		temp = cJSON_CreateNumber(temperatures_list[i]);
//		cJSON_AddItemToArray(temp_result, temp);
//	}

	cJSON *temp = NULL;
	temp = cJSON_CreateNumber(last_temperature);
	cJSON_AddItemToArray(temp_result, temp);

	cJSON_AddItemToObject(root, "temperatures", temp_result);

	cJSON_AddNumberToObject(root, "factor", roundTemperature(factor_temp));
	//cJSON_AddItemToArray(root, element);

	char *temperatures_json = cJSON_Print(root);
	ESP_LOGI("JSON", "\n%s\n\n", temperatures_json);
	cJSON_Delete(root);

	locked_list = false;

	return temperatures_json;
}

esp_err_t check_temperature_start_task() {
	if (check_temperature_task_handle == NULL) {

		xTaskCreate(check_temperature_task, "check_temperature", 1024 * 5, NULL,
		configMAX_PRIORITIES - 5, &check_temperature_task_handle);

		return ESP_OK;
	}

	return ESP_FAIL;
}

esp_err_t check_temperature_stop_task() {
	if (check_temperature_task_handle != NULL) {

		vTaskDelete(check_temperature_task_handle);

		return ESP_OK;
	}

	return ESP_FAIL;
}
