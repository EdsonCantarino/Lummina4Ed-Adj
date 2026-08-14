#include "include/pcf8574_driver.h"
#include "include/led_uv.h"
#include "include/task_manager.h"

static const gpio_num_t I2C_MASTER_SCL_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SCL;
static const gpio_num_t I2C_MASTER_SDA_GPIO = (gpio_num_t) CONFIG_I2C_MASTER_SDA;

PCF8574 led_uv(I2C_LED_UV_ADDRESS, I2C_MASTER_SDA_GPIO, I2C_MASTER_SCL_GPIO);

void led_uv_on(int led) {
	//	P0 Cavidade 3
	//	P1 Cavidade 1
	//	P2 Cavidade 4
	//	P3 Cavidade 2
	if (led == 2) {
		led_uv.digital_write(P3, 1);
	} else if (led == 3) {
		led_uv.digital_write(P0, 1);
	} else if (led == 4) {
		led_uv.digital_write(P2, 1);
	} else {
		led_uv.digital_write(P1, 1);
	}
}

void led_uv_off(int led) {
	//	P0 Cavidade 3
	//	P1 Cavidade 1
	//	P2 Cavidade 4
	//	P3 Cavidade 2

	if (led == 2) {
		led_uv.digital_write(P3, 0);
	} else if (led == 3) {
		led_uv.digital_write(P0, 0);
	} else if (led == 4) {
		led_uv.digital_write(P2, 0);
	} else {
		led_uv.digital_write(P1, 0);
	}
}

void led_uv_set_all(PCF8574 *uv, int state) {
	for (int i = 0; i < 4; i++) {
		uv->digital_write(i, state);
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void led_uv_clear_all(PCF8574 *uv) {
	for (int i = 0; i < 4; i++) {
		uv->digital_write(i, 1);
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void led_uv_clear_all() {
	led_uv_set_all(&led_uv, 1);
}

void led_uv_setup() {
	led_uv.begin();
	led_uv.set_digital_read_forced_mask(true);

	led_uv.pin_mode(P0, OUTPUT);
	led_uv.pin_mode(P1, OUTPUT);
	led_uv.pin_mode(P2, OUTPUT);
	led_uv.pin_mode(P3, OUTPUT);

	// 1 On - 0 Off
	led_uv.digital_write(P0, 0);
	led_uv.digital_write(P1, 0);
	led_uv.digital_write(P2, 0);
	led_uv.digital_write(P3, 0);

	led_uv.digital_write(P0, 1); // Cavidade 3
	led_uv.digital_write(P1, 1); // Cavidade 1
	led_uv.digital_write(P2, 1); // Cavidade 4
	led_uv.digital_write(P3, 1); // Cavidade 2

	// Era 3000ms - prendia o boot inteiro por 3s so pra esse flash de
	// autoteste. 300ms ja da pra enxergar o flash.
	vTaskDelay(pdMS_TO_TICKS(300));

	// 1 On - 0 Off
	led_uv.digital_write(P0, 0);
	led_uv.digital_write(P1, 0);
	led_uv.digital_write(P2, 0);
	led_uv.digital_write(P3, 0);
}

