#include "pcf8574.h"
#include "include/pcf8574_driver.h"

PCF8574::PCF8574() { // @suppress("Class members should be properly initialized")

}

PCF8574::PCF8574(uint8_t address) { // @suppress("Class members should be properly initialized")
	_address = address;
}

PCF8574::PCF8574(uint8_t address, int sda, int scl) { // @suppress("Class members should be properly initialized")
	_address = address;
	_sda = sda;
	_scl = scl;
}

void PCF8574::begin() {
	memset(&i2c, 0, sizeof(i2c_dev_t));
	ESP_ERROR_CHECK(
			pcf8574_init_desc(&i2c, _address, (i2c_port_t)0, (gpio_num_t )_sda,
					(gpio_num_t )_scl));

}

void PCF8574::set_digital_read_forced_mask(bool force){
	PCF8574::forced_read_mask = force;
}

uint8_t PCF8574::digital_read(uint8_t pin) {
	uint8_t value = (BV(pin) & readModePullUp) ? HIGH : LOW;

	if ((((BV(pin) & (readModePullDown & byteBuffered)) > 0)
			|| (BV(pin) & (readModePullUp & ~byteBuffered)) > 0)) {
		if ((BV(pin) & byteBuffered) > 0) {
			value = HIGH;
		} else {
			value = LOW;
		}
	} else {
		uint8_t read_value;

		esp_err_t err = pcf8574_port_read(&i2c, &read_value);

		//printf("%d\n", read_value);

		if ((readModePullDown & read_value) > 0
				or (readModePullUp & ~read_value) > 0) {
			//printf(" -------- CHANGE --------- ");
			byteBuffered = (byteBuffered & ~readMode) | read_value;
			if ((BV(pin) & byteBuffered) > 0) {
				value = HIGH;
			} else {
				value = LOW;
			}
		}

	}

	if ((BV(pin) & readModePullDown) and value == HIGH) {
		byteBuffered = BV(pin) ^ byteBuffered;
	} else if ((BV(pin) & readModePullUp) and value == LOW) {
		byteBuffered = BV(pin) ^ byteBuffered;
	} else if (BV(pin) & writeByteBuffered) {
		value = HIGH;
	}

	return value;
}

esp_err_t PCF8574::digital_write(uint8_t pin, uint8_t value) {
	if (value == HIGH) {
		writeByteBuffered = writeByteBuffered | BV(pin);
		byteBuffered = writeByteBuffered | BV(pin);
	} else {
		writeByteBuffered = writeByteBuffered & ~BV(pin);
		byteBuffered = writeByteBuffered & ~BV(pin);
	}

	byteBuffered = (writeByteBuffered & writeMode) | (resetInitial & readMode);

	if (forced_read_mask) {
		byteBuffered = byteBuffered | 0xF0;
	}

	esp_err_t err = pcf8574_port_write(&i2c, byteBuffered);

	byteBuffered = (writeByteBuffered & writeMode) | (initialBuffer & readMode);

	return err;
}

void PCF8574::set_val(uint8_t pin, uint8_t value) {
	if (value == HIGH) {
		writeByteBuffered = writeByteBuffered | BV(pin);
		byteBuffered = writeByteBuffered | BV(pin);
	} else {
		writeByteBuffered = writeByteBuffered & ~BV(pin);
		byteBuffered = writeByteBuffered & ~BV(pin);
	}

}
bool PCF8574::digital_write_all(PCF8574::DigitalInput digitalInput) {

	PCF8574::set_val(P0, digitalInput.p0);
	PCF8574::set_val(P1, digitalInput.p1);
	PCF8574::set_val(P2, digitalInput.p2);
	PCF8574::set_val(P3, digitalInput.p3);
	PCF8574::set_val(P4, digitalInput.p4);
	PCF8574::set_val(P5, digitalInput.p5);
	PCF8574::set_val(P6, digitalInput.p6);
	PCF8574::set_val(P7, digitalInput.p7);

	return digital_write_all_bytes(writeByteBuffered);
}

bool PCF8574::digital_write_all_bytes(uint8_t allpins) {
	writeByteBuffered = allpins;
	byteBuffered = (writeByteBuffered & writeMode) | (resetInitial & readMode);

	esp_err_t err = pcf8574_port_write(&i2c, byteBuffered | 0xF0);

	byteBuffered = (writeByteBuffered & writeMode) | (initialBuffer & readMode);

	return err == ESP_OK;
}

esp_err_t PCF8574::pin_mode(uint8_t pin, uint8_t mode) {
	if (mode == OUTPUT) {
		writeMode = writeMode | BV(pin);

		readMode = readMode & ~BV(pin);
		readModePullDown = readModePullDown & ~BV(pin);
		readModePullUp = readModePullUp & ~BV(pin);

	} else if (mode == INPUT) {
		writeMode = writeMode & ~BV(pin);

		readMode = readMode | BV(pin);
		readModePullDown = readModePullDown | BV(pin);
		readModePullUp = readModePullUp & ~BV(pin);
	} else {
		ESP_LOGE(TAG, "Mode non supported by PCF8574");
	}

	return ESP_OK;
}

