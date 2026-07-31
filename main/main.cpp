#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif

#include "rom/ets_sys.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "include/task_manager.h"
#include "include/rtc_ds1302.h"
#include "include/temperature.h"
#include "include/heater.h"

#include "include/task_manager.h"
#include "include/led_panel.h"
#include "include/i2c_driver.h"
#include "include/keyboard.h"
#include "include/buzzer.h"
#include "include/ampoule_sensor.h"
#include "include/led_uv.h"
#include "include/ampoules.h"
#include "include/light_sensor.h"
#include "include/ampoule_test.h"
#include "advanced_config.h"
#include "ampoule_history.h"

#include "include/printer.h"
#include "include/nvs_utils.h"

#include "include/wifi_ap.h"
#include "include/app_httpd.h"
#include "include/http_app.h"

#include "include/security.h"

#include "usb_printer.h"

// #include "task_monitor.h"

static const char *TAG = "LIMMINA4";

#include "include/coredump_to_server.h"

// uint8_t global_var;
//COREDUMP_DRAM_ATTR uint8_t global_var;

LinkedList<string> reset_reason_list = LinkedList<string>();

static esp_err_t _coredump_to_server_begin_cb(void *priv) {
	ets_printf("================= CORE DUMP START =================\r\n");
	return ESP_OK;
}

static esp_err_t _coredump_to_server_end_cb(void *priv) {
	ets_printf("================= CORE DUMP END ===================\r\n");
	return ESP_OK;
}

static esp_err_t _coredump_to_server_write_cb(void *priv,
		char const *const str) {
	ets_printf("%s\r\n", str);
	return ESP_OK;
}

void add_reset_reason(string r) {
	reset_reason_list.add(r);

	if (reset_reason_list.size() > 20) {
		reset_reason_list.shift();
	}
}

string convert_reset_reason_to_list() {
	int size = reset_reason_list.size();

	string shist = "";

	for (int i = 0; i < size; i++) {

		if (shist != "") {
			shist.append(";");
		}

		shist.append(reset_reason_list[i]);
	}

	return shist;
}

esp_err_t set_reset_reason_string(string cpu1, string cpu2, bool auto_save,
		bool auto_load) {

	LinkedList<string> list = load_reset_reason();

	for (int i = 0; i < list.size(); i++) {
		add_reset_reason(list[i]);
	}

	add_reset_reason(cpu1);
	add_reset_reason(cpu2);

	if (auto_save) {

		string s = convert_reset_reason_to_list();

		esp_err_t err = save_reset_reason(s);

		return err;
	}

	return ESP_OK;
}

void init() {
	esp_err_t err = nvs_init();

	if (err != ESP_OK)
		ESP_LOGE(TAG, "Error (%s) init NVS!\n", esp_err_to_name(err));

	err = save_restart_counter();

	if (err != ESP_OK)
		ESP_LOGE(TAG, "Error (%s) saving restart counter to NVS!\n",
				esp_err_to_name(err));
}

void print_reset_reason(int reason) {
	switch (reason) {
	case 1:
		printf("POWERON_RESET");
		break; /**<1,  Vbat power on reset*/
	case 3:
		printf("RTC_SW_SYS_RESET");
		break; /**<3,  Software reset digital core*/
	case 4:
		printf("OWDT_RESET");
		break; /**<4,  Legacy watch dog reset digital core*/
	case 5:
		printf("DEEPSLEEP_RESET");
		break; /**<5,  Deep Sleep reset digital core*/
	case 6:
		printf("SDIO_RESET");
		break; /**<6,  Reset by SLC module, reset digital core*/
	case 7:
		printf("TG0WDT_SYS_RESET");
		break; /**<7,  Timer Group0 Watch dog reset digital core*/
	case 8:
		printf("TG1WDT_SYS_RESET");
		break; /**<8,  Timer Group1 Watch dog reset digital core*/
	case 9:
		printf("RTCWDT_SYS_RESET");
		break; /**<9,  RTC Watch dog Reset digital core*/
	case 10:
		printf("INTRUSION_RESET");
		break; /**<10, Instrusion tested to reset CPU*/
	case 11:
		printf("TG0WDT_CPU_RESET");
		break; /**<11, Time Group0 reset CPU*/
	case 12:
		printf("RTC_SW_CPU_RESET");
		break; /**<12, Software reset CPU*/
	case 13:
		printf("RTCWDT_CPU_RESET");
		break; /**<13, RTC Watch dog Reset CPU*/
	case 14:
		printf("EXT_CPU_RESET");
		break; /**<14, for APP CPU, reseted by PRO CPU*/
	case 15:
		printf("RTCWDT_BROWN_OUT_RESET");
		break;/**<15, Reset when the vdd voltage is not stable*/
	case 16:
		printf("RTCWDT_RTC_RESET");
		break; /**<16, RTC Watch dog reset digital core and rtc module*/
	case 17:
		printf("TG1WDT_CPU_RESET");
		break;/**<17, Time Group1 reset CPU*/
	case 18:
		printf("SUPER_WDT_RESET");
		break;/**<18, super watchdog reset digital core and rtc module*/
	case 19:
		printf("GLITCH_RTC_RESET");
		break;/**<19, glitch reset digital core and rtc module*/
	case 20:
		printf("EFUSE_RESET");
		break;/**<20, efuse reset digital core*/
	case 21:
		printf("USB_UART_CHIP_RESET");
		break;/**<21, usb uart reset digital core */
	case 22:
		printf("USB_JTAG_CHIP_RESET");
		break;/**<22, usb jtag reset digital core */
	case 23:
		printf("POWER_GLITCH_RESET");
		break;/**<23, power glitch reset digital core and rtc module*/
	default:
		printf("NO_MEAN");
	}

	printf("\n");
}

void verbose_print_reset_reason(int reason) {
	switch (reason) {
	case 1:
		printf("Vbat power on reset");
		break;
	case 3:
		printf("Software reset digital core");
		break;
	case 4:
		printf("Legacy watch dog reset digital core");
		break;
	case 5:
		printf("Deep Sleep reset digital core");
		break;
	case 6:
		printf("Reset by SLC module, reset digital core");
		break;
	case 7:
		printf("Timer Group0 Watch dog reset digital core");
		break;
	case 8:
		printf("Timer Group1 Watch dog reset digital core");
		break;
	case 9:
		printf("RTC Watch dog Reset digital core");
		break;
	case 10:
		printf("Instrusion tested to reset CPU");
		break;
	case 11:
		printf("Time Group0 reset CPU");
		break;
	case 12:
		printf("Software reset CPU");
		break;
	case 13:
		printf("RTC Watch dog Reset CPU");
		break;
	case 14:
		printf("for APP CPU, reseted by PRO CPU");
		break;
	case 15:
		printf("Reset when the vdd voltage is not stable");
		break;
	case 16:
		printf("RTC Watch dog reset digital core and rtc module");
		break;
	case 17:
		printf("Time Group1 reset CPU");
		break;
	case 18:
		printf("super watchdog reset digital core and rtc module");
		break;
	case 19:
		printf("glitch reset digital core and rtc module");
		break;
	case 20:
		printf("efuse reset digital core");
		break;
	case 21:
		printf("usb uart reset digital core");
		break;
	case 22:
		printf("usb jtag reset digital core");
		break;
	case 23:
		printf("power glitch reset digital core and rtc module");
		break;

	default:
		printf("NO_MEAN");
	}

	printf("\n");
}

void print_msg(const char *msg, int linesBefore = 0, int linesAfter = 0) {

	if (linesBefore > 0) {
		for (int i = 0; i < linesBefore; i++) {
			printf("\n");
		}
	}

	printf(msg);

	if (linesAfter > 0) {
		for (int i = 0; i < linesAfter; i++) {
			printf("\n");
		}
	}
}

void print_reset_reason() {
	print_msg("########## Motivo do reset do dispositivo ##########", 1, 2);

	RESET_REASON r_cpu1 = rtc_get_reset_reason(0);
	RESET_REASON r_cpu2 = rtc_get_reset_reason(1);

	set_reset_reason_string(to_string(r_cpu1), to_string(r_cpu2), true, true);

	for (int i = 0; i < reset_reason_list.size(); i++) {
		printf("\nCPU0 reset reason:");
		print_reset_reason(atoi(reset_reason_list[i].c_str()));
		verbose_print_reset_reason(atoi(reset_reason_list[i].c_str()));

		i++;

		printf("\nCPU1 reset reason:");
		print_reset_reason(atoi(reset_reason_list[i].c_str()));
		verbose_print_reset_reason(atoi(reset_reason_list[i].c_str()));
	}

	printf("\n");
}

void setup() {
	//nvs_flash_init();
	init();

	coredump_to_server_config_t coredump_cfg = { .start =
			_coredump_to_server_begin_cb, .end = _coredump_to_server_end_cb,
			.write = _coredump_to_server_write_cb, .priv = NULL, };
	coredump_to_server(&coredump_cfg);

	//assert(0);

	print_reset_reason();

	// 2. Start task monitor:
	//task_monitor();

	// Inicia o drive i2c
	// Importante fazer antes de chamar os setups para o i2c
	ESP_ERROR_CHECK(i2cdev_init());

	task_manager_setup();

	// Carrega as configuracoes avancadas (itens 1 a 5) da NVS antes de
	// qualquer outro setup que dependa delas.
	advanced_config_load();
	ampoule_history_load();

	temperature_setup();

	heater_setup();

	// RTC Setup
	rtc_ds1302_setup();

	buzzer_main();

	led_panel_setup();
	led_uv_setup();
	keyboard_setup();
	ampoule_sensor_setup();

	// Aplica o estado de cavidades habilitadas/desabilitadas (item 4)
	// depois que as ampolas ja foram inicializadas.
	ampoule_apply_cavity_enabled_config();

	light_sensor_setup();
	//light_sensor_main();

	// Iniciar o teclado antes dos leds
	keyboard_main();

	// Iniciar sempre depois do teclado
	led_panel_main();

	// Inicia o sensor das ampoulas
	ampoule_sensor_main();

	vTaskDelay(pdMS_TO_TICKS(1000));

	// inicia o aquecedor.
	heater_start();

	ampoules_tests_setup();
	ampoules_tests_main();

	//vTaskDelay(pdMS_TO_TICKS(3000));

	// Aplica��o WEB
	wifi_ap_main(getPassword().c_str());
	/* start http server */
	http_app_start(true);
	/* start http server */
	//http_app_start(false);

	printer_setup();

	usb_printer_setup();
}

extern "C" void app_main() {
	//WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

	setup();
}
