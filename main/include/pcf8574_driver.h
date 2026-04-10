#ifndef PCF8574_DRIVER_h
#define PCF8574_DRIVER_h

#include <esp_log.h>
#include <esp_err.h>
#include <esp_idf_lib_helpers.h>

#include <stddef.h>
#include <string.h>
#include <i2cdev.h>

#include <string.h>
#include <math.h>

#define DEFAULT_SDA SDA;
#define DEFAULT_SCL SCL;

#define P0  	0
#define P1  	1
#define P2  	2
#define P3  	3
#define P4  	4
#define P5  	5
#define P6  	6
#define P7  	7

#define LOW               0x00
#define HIGH              0x1

#define INPUT             0x01
#define OUTPUT            0x03

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)
#define BV(x) (1UL << (x))

class PCF8574 {
public:

	PCF8574();
	PCF8574(uint8_t address);
	PCF8574(uint8_t address, int sda, int scl);

	void begin();
	void reset();

	void set_digital_read_forced_mask(bool force);

	esp_err_t pin_mode(uint8_t pin, uint8_t mode);

	uint8_t digital_read(uint8_t pin);

	struct DigitalInput {
		uint8_t p0;
		uint8_t p1;
		uint8_t p2;
		uint8_t p3;
		uint8_t p4;
		uint8_t p5;
		uint8_t p6;
		uint8_t p7;
	} digitalInput;

	//DigitalInput digital_read_all(void);

	bool digital_write_all(uint8_t digitalInput);
	bool digital_write_all(PCF8574::DigitalInput digitalInput);

	uint8_t digital_read_all(void);

	esp_err_t digital_write(uint8_t pin, uint8_t value);
	esp_err_t digital_write(uint8_t pin, uint8_t value, bool force_io);
private:
	const char *TAG = "PCF8574";

	i2c_dev_t i2c;
	uint8_t _address;

	int _sda;
	int _scl;

	bool forced_read_mask = false;

	uint8_t writeMode = 0b00000000;
	uint8_t readMode = 0b00000000;
	uint8_t writeModeUp = 0b00000000;
	uint8_t readModePullUp = 0b00000000;
	uint8_t readModePullDown = 0b00000000;
	uint8_t byteBuffered = 0b00000000;
	uint8_t resetInitial = 0b00000000;
	uint8_t initialBuffer = 0b00000000;

	uint8_t writeByteBuffered = 0b00000000;

	volatile uint8_t encoderValues = 0b00000000;

	uint8_t prevNextCode = 0;
	uint16_t store = 0;

	void set_val(uint8_t pin, uint8_t value);
	bool digital_write_all_bytes(uint8_t allpins);
};

#endif
