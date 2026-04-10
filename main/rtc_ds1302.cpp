#include "include/rtc_ds1302.h"
#include <chrono>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include <stdlib.h>

#define YEAR CONFIG_DS1302_TIMESTAMP_YEAR
#define MOUNTH CONFIG_DS1302_TIMESTAMP_MOUNTH
#define DAY CONFIG_DS1302_TIMESTAMP_DAY
#define HOUR CONFIG_DS1302_TIMESTAMP_HOUR
#define MINUTE CONFIG_DS1302_TIMESTAMP_MINUTE
#define SECOND CONFIG_DS1302_TIMESTAMP_SECOND

static const char *TAG = "DS1302";

static const gpio_num_t CE_GPIO = (gpio_num_t)CONFIG_DS1302_CE_GPIO;
static const gpio_num_t SCLK_GPIO = (gpio_num_t)CONFIG_DS1302_SCLK_GPIO;
static const gpio_num_t IO_GPIO = (gpio_num_t)CONFIG_DS1302_IO_GPIO;

ds1302_t dev = {.ce_pin = CE_GPIO, .io_pin = IO_GPIO, .sclk_pin = SCLK_GPIO};

void rtc_ds1302_print_date_time(const struct tm time)
{
	char buffer[72];

	sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", time.tm_mday, (time.tm_mon + 1),
			(time.tm_year + 1900), time.tm_hour, time.tm_min, time.tm_sec);

	printf("\tReceived Date & Time: %s\n", buffer);
}

void rtc_ds1302_adjust_timestamp(const struct tm time)
{
	char buffer[72];

	ESP_ERROR_CHECK(ds1302_set_time(&dev, &time));
	ESP_ERROR_CHECK(ds1302_start(&dev, true));

	rtc_ds1302_get_date_time(buffer);
}

bool rtc_ds1302_check_date(int m, int d, int y)
{
	// gregorian dates started in 1582
	if (!(1582 <= y)) // comment these 2 lines out if it bothers you
		return false;
	if (!(1 <= m && m <= 12))
		return false;
	if (!(1 <= d && d <= 31))
		return false;
	if ((d == 31) && (m == 2 || m == 4 || m == 6 || m == 9 || m == 11))
		return false;
	if ((d == 30) && (m == 2))
		return false;
	if ((m == 2) && (d == 29) && (y % 4 != 0))
		return false;
	if ((m == 2) && (d == 29) && (y % 400 == 0))
		return true;
	if ((m == 2) && (d == 29) && (y % 100 == 0))
		return false;
	if ((m == 2) && (d == 29) && (y % 4 == 0))
		return true;

	return true;
}

tm rtc_ds1302_adjust_date(tm date) {
    if (!rtc_ds1302_check_date(date.tm_mon + 1, date.tm_mday, date.tm_year + 1900)) {
        time_t t = mktime(&date);
        t += 24 * 3600; // Adiciona um dia (24 horas em segundos)
        tm adjustedDate = *localtime(&t);

        ds1302_set_time(&dev, &adjustedDate);

        return adjustedDate;
    }
    return date;
}

void rtc_ds1302_get_date_time(char *buffer)
{
	struct tm timeinfo;

	ds1302_get_time(&dev, &timeinfo);

	timeinfo = rtc_ds1302_adjust_date(timeinfo);

	sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon + 1,
			(timeinfo.tm_year + 1900), timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

	ESP_LOGI(TAG, "Date & Time: %s", buffer);
}

void rtc_ds1302_setup()
{

	ESP_LOGI(TAG, "Configuring DS1302");

	ESP_LOGI(TAG, "GPIO CE: %02d, GPIOSCLK: %02d, GPIOIO: %02d", CE_GPIO,
			 SCLK_GPIO, IO_GPIO);

	ESP_ERROR_CHECK(ds1302_init(&dev));
	ESP_ERROR_CHECK(ds1302_set_write_protect(&dev, false));

	// ESP_ERROR_CHECK(ds1302_start(&dev, true));

	ESP_LOGI(TAG, "DS1302 Configured!");
}

esp_err_t rtc_write_sram_memory(int value)
{
	uint8_t len = sizeof(int);
	uint8_t buf[len];
	uint8_t offset = 0;

	memcpy(buf, &value, len);

	// Write data to SRAM
	esp_err_t err = ds1302_write_sram(&dev, offset, buf, len);

	return err;
}

int rtc_read_sram_memory()
{
	uint8_t len = sizeof(int);
	uint8_t buf[len];
	uint8_t offset = 0;

	esp_err_t err = ds1302_read_sram(&dev, offset, buf, len);
	if (err != ESP_OK)
	{
		// Handle error
	}

	if (len == 0)
	{
		// Buffer is empty
		return 0;
	}

	// Get the integer value from the buffer
	int32_t read_value;
	memcpy(&read_value, buf, len);

	// Check if the value is valid
	if (read_value == 0 || read_value >= 2000000)
	{
		// No value in SRAM memory

		printf("Retornando o valor 0\n");

		return 0;
	}

	// Print the value
	printf("Value: %ld\n", read_value);

	return read_value;
}
