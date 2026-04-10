#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <cmath>

#include "include/temperature.h"
#include "include/task_manager.h"
#include "LinkedList.h"
#include "cJSON.h"
#include "include/nvs_utils.h"

static const gpio_num_t SENSOR_GPIO = (gpio_num_t) CONFIG_DS18X20_ONEWIRE_GPIO;
static const int MAX_SENSORS = CONFIG_DS18X20_MAX_SENSORS;
static const int RESCAN_INTERVAL = 8;
static const uint32_t LOOP_DELAY_MS = 5000;

static const char *TAG = "TEMPERATURE";

// TaskHandlers
static TaskHandle_t check_temperature_task_handle;

// Prototipos
void check_temperature_task(void *pvParameter);
esp_err_t send_temperature_buffer(float *temperature);
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
		res = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS,
				&sensor_count);
		if (res != ESP_OK) {
			ESP_LOGE(TAG, "Sensors scan error %d (%s)", res,
					esp_err_to_name(res));

			xEventGroupClearBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);
			continue;
		}

		if (!sensor_count) {
			ESP_LOGW(TAG, "No sensors detected!");

			xEventGroupClearBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);
			continue;
		}

		ESP_LOGD(TAG, "%d sensors detected", sensor_count);
		xEventGroupSetBits(sensors_event_group, SENSOR_TEMPERATURE_BIT);

		// If there were more sensors found than we have space to handle,
		// just report the first MAX_SENSORS..
		if (sensor_count > MAX_SENSORS)
			sensor_count = MAX_SENSORS;

		// Do a number of temperature samples, and print the results.
		for (int i = 0; i < RESCAN_INTERVAL; i++) {
			//ESP_LOGD(TAG, "Measuring...");

			if (count == 0) {
				get_calibration_factor(factor_temp);
			}

			//ESP_LOGI(TAG, "%.2f fator de calibracao", factor_temp);

			//factor_temp = 0.0f;

			count++;

			if (count > 60) {
				count = 0;
			}

			res = ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs,
					sensor_count, temps);
			if (res != ESP_OK) {
				ESP_LOGE(TAG, "Sensors read error %d (%s)", res,
						esp_err_to_name(res));
				continue;
			}

			for (int j = 0; j < sensor_count; j++) {
				float temp_c = temps[j];
				//float temp_f = (temp_c * 1.8) + 32;

				float temp_factor = roundTemperature(temp_c) + roundTemperature(factor_temp);

				last_temperature = temp_factor;

				//temp_factor = 69.5f;

//				ESP_LOGI(TAG,
//						"Temperatura: %.3f + Fator de Calibracao: %.3f = %.3f",
//						temp_c, factor_temp, temp_factor);

				//printf("%.3f\n", temp_factor);
				//printf("%.1f\n\n", factor_temp);

				send_temperature_buffer(&temp_factor);			}

			// Wait for a little bit between each sample (note that the
			// ds18x20_measure_and_read_multi operation already takes at
			// least 750ms to run, so this is on top of that delay).
			vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
		}
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

esp_err_t send_temperature_buffer(float *temperature) {
	size_t xBytesSent;

//	ESP_LOGI(TAG, "Temperature Send: %.3f", *temperature);

	xBytesSent = xMessageBufferSend(temperature_message_buffer,
			(void* )&temperature, sizeof(float), 0);

	if (xBytesSent != sizeof(temperature)) {
		return ESP_FAIL;
	}

	return ESP_OK;
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
