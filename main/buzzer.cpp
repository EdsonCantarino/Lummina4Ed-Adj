#include "include/buzzer.h"

#define HIGH 1
#define LOW 0

#define BUZZER_TIMEOUT CONFIG_BUZZER_ACTIVATED
static const gpio_num_t BUZZER_GPIO = (gpio_num_t) CONFIG_BUZZER_GPIO;

static const char *TAG = "Buzzer";

static TimerHandle_t shutdown_buzzer_timer;
static SemaphoreHandle_t shutdown_semaphore;

volatile bool is_alarm = false;
volatile bool is_alert = false;

bool is_buzzer_on_off = false;
bool is_buzzer_alert_on_off = false;

void set_buzzer_on_off() {
	is_buzzer_on_off = !is_buzzer_on_off;

	printf("Status do alarme: %d! - 1 = Ligado, 0 = Desligado\n\n", is_buzzer_on_off);
}

void set_buzzer_on_off(bool status) {
	is_buzzer_on_off = status;
}

void set_buzzer_alert_on_off(bool status) {
	is_buzzer_alert_on_off = status;
}

void set_alarm(bool status) {
	is_alarm = status;
}

bool get_buzzer_on_off_status() {
	return is_buzzer_on_off;
}

void buzzer_alarm_task(void *parameter) {
	const TickType_t xBlockTime = pdMS_TO_TICKS(1000);

	while (true) {
		if (is_alarm) {
			//buzzer_alarm();

			for (int i = 0; i < 8; i++) {
				buzzer_on();
				vTaskDelay(pdMS_TO_TICKS(50));
				buzzer_off();
				vTaskDelay(pdMS_TO_TICKS(50));
			}

			buzzer_on();
			vTaskDelay(pdMS_TO_TICKS(500));
			buzzer_off();

//			buzzer_on();
//			vTaskDelay(pdMS_TO_TICKS(2000));
//			buzzer_off();
		}

		vTaskDelay(xBlockTime);
	}

	vTaskDelete(NULL);
}

void buzzer_alert_task(void *parameter) {
	const TickType_t xBlockTime = pdMS_TO_TICKS(1000);

	while (true) {
		if (is_buzzer_on_off && is_buzzer_alert_on_off) {
			//buzzer_alert(true);
			buzzer_continuous(pdMS_TO_TICKS(500));
		}

		vTaskDelay(xBlockTime);
	}

	vTaskDelete(NULL);
}

void shutdown_buzzer(TimerHandle_t xTimer) {
	ESP_LOGI(TAG, "Stopping Buzzer");

	gpio_set_level(BUZZER_GPIO, LOW);

	xSemaphoreGive(shutdown_semaphore);
}

void buzzer_alarm() {
	buzzer_on();
	vTaskDelay(pdMS_TO_TICKS(50));
	buzzer_off();
}

void buzzer_continuous(TickType_t delay) {
	buzzer_on();
	vTaskDelay(delay);
	buzzer_off();
}

void buzzer_alert(bool inverted) {
	TickType_t t = pdMS_TO_TICKS(75);
	TickType_t p = pdMS_TO_TICKS(150);

	for (int i = 0; i < 3; i++) {
		buzzer_on();
		vTaskDelay(inverted ? p : t);
		buzzer_off();
		vTaskDelay(inverted ? t : p);
	}
}

void buzzer_on() {
	gpio_set_level(BUZZER_GPIO, HIGH);
}

void buzzer_off() {
	gpio_set_level(BUZZER_GPIO, LOW);
}

void configure_buzzer(void) {
	gpio_reset_pin(BUZZER_GPIO);

	gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
}

void buzzer_main() {

	configure_buzzer();

	xTaskCreate(buzzer_alarm_task, "BUZZER_ALARM", 1024 * 5, NULL,
	configMAX_PRIORITIES - 5, NULL);

	xTaskCreate(buzzer_alert_task, "BUZZER_ALERT", 1024 * 5, NULL,
	configMAX_PRIORITIES - 10, NULL);

	// Desativado para testes
//	shutdown_buzzer_timer = xTimerCreate("Buzzer shutdown timer",
//	BUZZER_TIMEOUT / portTICK_PERIOD_MS, pdFALSE, NULL, shutdown_buzzer);
//	shutdown_semaphore = xSemaphoreCreateBinary();
}

esp_err_t start_buzzer() {
	ESP_LOGI(TAG, "Starting Buzzer");

	if (!xTimerIsTimerActive(shutdown_buzzer_timer)) {
		xTimerStart(shutdown_buzzer_timer, portMAX_DELAY);

		gpio_set_level(BUZZER_GPIO, HIGH);

		ESP_LOGI(TAG, "Successfully. Buzzer is activated");
		return ESP_OK;
	}

	ESP_LOGE(TAG, "Fail! Buzzer is already active.");
	return ESP_FAIL;
}

esp_err_t stop_buzzer() {
	ESP_LOGI(TAG, "Stopping Buzzer");

	if (xTimerIsTimerActive(shutdown_buzzer_timer)) {

		xTimerStop(shutdown_buzzer_timer, 0);
		xSemaphoreGive(shutdown_semaphore);

		gpio_set_level(BUZZER_GPIO, LOW);

		ESP_LOGI(TAG, "Successfully. Buzzer is stopped");
		return ESP_OK;
	}

	ESP_LOGE(TAG, "Fail! Buzzer is not active.");
	return ESP_FAIL;
}
