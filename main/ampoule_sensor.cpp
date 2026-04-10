#include "include/ampoule_sensor.h"
#include "include/pcf8574_driver.h"
#include "include/task_manager.h"
#include "include/buzzer.h"
#include "include/ampoule_test.h"

static const gpio_num_t I2C_MASTER_SCL_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SCL;
static const gpio_num_t I2C_MASTER_SDA_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SDA;

PCF8574 ampoule(I2C_AMPOULES_ADDRESS, I2C_MASTER_SDA_GPIO, I2C_MASTER_SCL_GPIO);

volatile bool is_ampoules_present_in_init = true;

bool check_if_ampoules_is_present_on_init() {
	return is_ampoules_present_in_init;
}

void ampoule_sensor_setup() {
	ampoule_test_setup();
	ampoule.begin();

	ampoule.pin_mode(P4, INPUT);
	ampoule.pin_mode(P5, INPUT);
	ampoule.pin_mode(P6, INPUT);
	ampoule.pin_mode(P7, INPUT);
}

void read_ampoules(void *pvParameter) {

	while (1) {

		uint8_t ampoule1 = ampoule.digital_read(P4);
		uint8_t ampoule2 = ampoule.digital_read(P5);
		uint8_t ampoule3 = ampoule.digital_read(P6);
		uint8_t ampoule4 = ampoule.digital_read(P7);

		bool is_ampoules = (ampoule1 == 0 || ampoule2 == 0 || ampoule3 == 0
				|| ampoule4 == 0);

		if (is_ampoules_present_in_init && is_ampoules) {

			//printf("\n\nAQUI: Ampola inserida, tem que remover\n\n");

		} else {
			is_ampoules_present_in_init = false;

			ampoule_set_status(ampoule1 == 0, ampoule2 == 0, ampoule3 == 0,
					ampoule4 == 0);
		}

		vTaskDelay(pdMS_TO_TICKS(300));
	}
}

void ampoule_sensor_main() {
	xTaskCreate(read_ampoules, "READ_AMPOULES",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, NULL);
}
