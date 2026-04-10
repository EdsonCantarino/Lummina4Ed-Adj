//#include "freertos/FreeRTOS.h"
//#include "esp_wifi.h"
//#include "esp_system.h"
//#include "esp_event.h"
//#include "driver/gpio.h"
//#include <stdio.h>
//#include <stdint.h>
//#include "freertos/task.h"
//#include "esp_log.h"
//#include "esp_err.h"
//
//#include "esp_spibus.hpp"
//#include "cs553x.h"
//
//#define SPI_MODE  0
//#define MISO_PIN  (gpio_num_t)11
//#define MOSI_PIN  (gpio_num_t)13
//#define SCLK_PIN  (gpio_num_t)12
//#define CS_PIN    (gpio_num_t)10
//
//#define SPI_CLOCK 1000000  // 1 MHz
//
//SPI_t &mySPI = vspi;  // vspi and hspi are the default objects
//
//void printBits(size_t const size, void const *const ptr);
//
//esp_err_t write_long(spi_device_handle_t &device, uint8_t cmd,
//		unsigned long data) {
//	uint8_t txdata[20];
//
//	txdata[0] = (data >> 24) & 0xFF;
//	txdata[1] = (data >> 16) & 0xFF;
//	txdata[2] = (data >> 8) & 0xFF;
//	txdata[3] = data & 0xFF;
//
//	ESP_ERROR_CHECK(mySPI.writeBytes(device, cmd, 4, txdata));
//
//	return ESP_OK;
//}
//
//unsigned long read_long(spi_device_handle_t &device, uint8_t cmd) {
//	uint8_t rxdata[20];
//
//	ESP_ERROR_CHECK(mySPI.readBytes(device, cmd, 8, rxdata));
//
//	unsigned long data = rxdata[0] | rxdata[1] << 8 | rxdata[2] << 8
//			| rxdata[3] << 8;
//
//	printf("%ld", data);
//
//	return data;
//}
//
//esp_err_t reset_spi(spi_device_handle_t &device) {
//	uint8_t buffer[20];
//
//	return write_long(device, CS553X_CMD_WRITE_CONF, 0x20000000);
//}
//
//esp_err_t reset_spi_done(spi_device_handle_t &device) {
//	unsigned long RV;
//	unsigned char RV_bit;
//
//	RV = read_long(device, CS553X_CMD_READ_CONF);
//	printBits(sizeof(RV), &RV);
//
//	RV_bit = RV && 0x10000000;
//	printBits(sizeof(RV_bit), &RV_bit);
//
//	if (RV_bit == 1) {
//		printf("Reset is Done\n");
//
//		write_long(device, CS553X_CMD_WRITE_CONF, 0x00000000);
//
//		return ESP_OK;
//	}
//
//	return ESP_FAIL;
//
//}
//
//void reset_serial(spi_device_handle_t &device) {
//	ESP_ERROR_CHECK(mySPI.writeByte(device, 0xff, 0xffffffff));
//	ESP_ERROR_CHECK(mySPI.writeByte(device, 0xff, 0xffffffff));
//	ESP_ERROR_CHECK(mySPI.writeByte(device, 0xff, 0xffffffff));
//	ESP_ERROR_CHECK(mySPI.writeByte(device, 0xff, 0xffffffff));
//}
//
//void init_cs553x(spi_device_handle_t &device) {
//
//	uint8_t buffer = { };
//
//	for (int i = 0; i < 15; i++) {
//		ESP_ERROR_CHECK(mySPI.writeByte(device, CS553X_SYNC1, buffer));
//	}
//
//	ESP_ERROR_CHECK(mySPI.writeByte(device, CS553X_SYNC0, buffer));
//}
//
////Print Binary for Any Datatype
//void printBits(size_t const size, void const *const ptr) {
//	unsigned char *b = (unsigned char*) ptr;
//	unsigned char byte;
//	int i, j;
//
//	for (i = size - 1; i >= 0; i--) {
//		for (j = 7; j >= 0; j--) {
//			byte = (b[i] >> j) & 1;
//			printf("%u", byte);
//		}
//	}
//	puts("");
//
//	printf("\n");
//}
//
//void cs553x_VREF(spi_device_handle_t &device) {
//	unsigned long vref;
//	unsigned char vref_bit;
//
//	write_long(device, CS553X_CMD_WRITE_CONF, 0x00000000);
//
//	vref = read_long(device, CS553X_CMD_READ_CONF);
//
//	printf("Config Setup: ");
//	printBits(sizeof(vref), &vref);
//
//	if (vref == 0x00000000)
//		vref_bit = 1;
//
//	printf("VREF must be 1 = ");
//	printBits(sizeof(vref_bit), &vref_bit);
//}
//
//void cs553x_config(spi_device_handle_t &device) {
//	unsigned long bip1;
//
//	bip1 = read_long(device, CS553X_CMD_READ_INDV_CH_SETUP1);
//
//	printf("INDV_CH_SETUP1: ");
//	printBits(sizeof(bip1), &bip1);
//
//	write_long(device, CS553X_CMD_WRITE_INDV_CH_SETUP1, 0x3200000);
//
//	bip1 = read_long(device, CS553X_CMD_READ_INDV_CH_SETUP1);
//
//	printf("INDV_CH_SETUP1_new: ");
//	printBits(sizeof(bip1), &bip1);
//}
//
//void cs553x_continuous_conversion(spi_device_handle_t &device) {
//	uint8_t buffer = { };
//
//	ESP_ERROR_CHECK(
//			mySPI.writeByte(device, CS553X_CMD_CONTINUOUS_CONV_CH_SETUP1, buffer));
//}
//
//void cs553x_single_conversion(spi_device_handle_t &device) {
//	uint8_t buffer = { };
//
//	ESP_ERROR_CHECK(
//			mySPI.writeByte(device, CS553X_CMD_SINGLE_CONV_CH_SETUP1, buffer));
//
//}
//
//char cs553x_readADC(spi_device_handle_t &device, unsigned char *buffer) {
//
//	while (gpio_get_level(MISO_PIN) == 1) {
//		//printf("MISO is 1\n");
//		return (0xFF);
//	}
//
//	uint8_t rxdata[20];
//
//	ESP_ERROR_CHECK(mySPI.writeByte(device, CS553X_NULL, 0x1000));
//
//	ESP_ERROR_CHECK(mySPI.readBytes(device, 0, 8, buffer));
//
//	unsigned long data = rxdata[0] | rxdata[1] << 8 | rxdata[2] << 8
//			| rxdata[3] << 8;
//
//	return (0x0);
//}
//
//spi_device_handle_t cs553x_setup(gpio_num_t miso, gpio_num_t mosi, gpio_num_t sclk,
//		gpio_num_t cs) {
//	printf("SPIbus Example \n");
//	fflush(stdout);
//
//	gpio_config_t io_conf;
//	io_conf.pull_down_en = (gpio_pulldown_t) 0;
//	io_conf.intr_type = (gpio_int_type_t) GPIO_PIN_INTR_DISABLE;
//	io_conf.pin_bit_mask = (1ULL << CS_PIN );
//	io_conf.mode = GPIO_MODE_OUTPUT;
//	io_conf.pull_up_en = (gpio_pullup_t) 0;
//	gpio_config(&io_conf);
//
//	spi_device_handle_t device;
//
//	ESP_ERROR_CHECK(mySPI.begin(MOSI_PIN, MISO_PIN, SCLK_PIN));
//	ESP_ERROR_CHECK(mySPI.addDevice(SPI_MODE, SPI_CLOCK, CS_PIN, &device));
//
//	printf("Iniciando CS5534 \n");
//	gpio_set_level(CS_PIN, 0);
//
//	vTaskDelay(2000 / portTICK_PERIOD_MS);
//
//	printf("sending...\n");
//	init_cs553x(device);
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//	printf("Init is Done\n");
//
//	printf("resetting...\n");
//	reset_spi(device);
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//
//	ESP_ERROR_CHECK(reset_spi_done(device));
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//
//	cs553x_VREF(device);
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//
//	cs553x_config(device);
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//
//	printf("All is Done\n");
//
//	//cs5534_continuous_conversion(device);
//	cs553x_single_conversion(device);
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//	printf("conversion is sent\n");
//
//	gpio_set_level(CS_PIN, 1);
//
//	return device;
//}
