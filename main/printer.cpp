#include "usb_printer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "include/led_panel.h"

#include "include/task_manager.h"
#include "LinkedList.h"
#include "thermal_printer.h"

#include "AmpouleSensor.h"
#include "include/ampoule_test.h"
#include "include/device_utils.h"
#include "include/nvs_utils.h"
#include "usb_class_driver.h"
#include "include/helper_utils.h"

#include "branding.h"
#include "ampoule_history.h"
#include <string>
#include <ctime>

using namespace std;

device_settings_t settings;

void print_test(string ampola, string id_test, string dt_inicio,
		string hr_inicio, string dt_fim, string hr_fim, int resultado,
		string ciclo, int temperature, string serial_number, string inst,
		string tempo_em_teste, string language);

void print_test_cancelled(string ampola, string id_test, string dt_inicio,
		string hr_inicio, string ciclo, int temperature, string serial_number,
		string inst, string tempo_em_teste, string language);

static string format_ts(uint32_t ts) {
	if (ts == 0)
		return "01/01/2000 00:00:00";

	time_t t = (time_t) ts;
	struct tm tm_val;
	localtime_r(&t, &tm_val);

	char buf[64];
	snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
			tm_val.tm_mday, tm_val.tm_mon + 1, tm_val.tm_year + 1900,
			tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);

	return string(buf);
}

void ampoule_history_record_to_result(const ampoule_history_record_t &rec,
		AmpouleTestResult &result) {

	char id_test_format[11];
	snprintf(id_test_format, sizeof(id_test_format), "%010lu",
			(unsigned long) rec.id_test);

	result.set_id_test(string(id_test_format));
	result.set_id(rec.cavidade);
	result.set_cicle((long) rec.ciclo_minutos * 60);

	result.set_date_time(format_ts(rec.ts_inicio), true);
	result.set_date_time(format_ts(rec.ts_fim), false);

	bool is_positived = rec.resultado == AMPOULE_RESULT_POSITIVE;
	bool is_cancelled = rec.resultado == AMPOULE_RESULT_CANCELLED;

	result.set_result(is_positived, is_cancelled);
	result.set_temperature(rec.temperatura);
}

void print_check_status(void *pvParameter) {
	bool statePrinterError = false;
	bool statePrinterConnected = false;
	for (;;) {

		if (!is_printer_connected()) {
			print_leds(1);
		} else {

			statePrinterConnected = is_setup_done() && !is_printer_error();
			print_leds(!statePrinterConnected);

			// Aceso conectada, apagada n�o conectada e piscando erro de impress�o.
			if (!is_setup_done()) {
				statePrinterError = !statePrinterError;
				print_leds(statePrinterError);
			}
		}

		// || is_printer_error()

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void print_ampoule_test_history(AmpouleTestResult &amp) {
	set_is_priting(true);

	string institution = get_institution();

	string serial_number = get_serial_number();
	string language = get_language();

	if (!amp.get_result_canceld()) {
		print_test(amp.get_id(), amp.get_id_test(), amp.get_date(true),
				amp.get_hour(true), amp.get_date(false), amp.get_hour(false),
				amp.get_result_positived() ? 1 : 2, amp.get_cicle(),
				amp.get_temperature(), serial_number, institution,
				amp.get_time_in_test(), language);
	} else {
		print_test_cancelled(amp.get_id(), amp.get_id_test(),
				amp.get_date(true), amp.get_hour(true), amp.get_cicle(),
				amp.get_temperature(), serial_number, institution,
				amp.get_time_in_test(), language);
	}

	set_is_priting(false);
}

void print_history() {

}

void print_ampoule_test(int id, bool is_cancelled = false) {

	string institution = get_institution();
	string serial_number = get_serial_number();
	string language = get_language();

	AmpouleTestResult amp = get_test_result(id);

	ESP_LOGI("", "Start Date & Time Formatted: %s",
			amp.get_formated_date_time(true, " | ").c_str());
	ESP_LOGI("", "End   Date & Time Formatted: %s",
			amp.get_formated_date_time(false, " | ").c_str());

	set_is_priting(true);

	if (!is_cancelled) {

		print_test(amp.get_id(), amp.get_id_test(), amp.get_date(true),
				amp.get_hour(true), amp.get_date(false), amp.get_hour(false),
				amp.get_result_positived() ? 1 : 2, amp.get_cicle(),
				amp.get_temperature(), serial_number, institution,
				amp.get_time_in_test(), language);
	} else {
		print_test_cancelled(amp.get_id(), amp.get_id_test(),
				amp.get_date(true), amp.get_hour(true), amp.get_cicle(),
				amp.get_temperature(), serial_number, institution,
				amp.get_time_in_test(), language);
	}

	set_is_priting(false);
}

void to_ampoule_test_result(string hist, AmpouleTestResult &result) {

	vector<string> xhist = str_split(hist, ";");

	result.set_id_test(xhist[0]);
	result.set_id(stoi(xhist[1]));

	result.set_cicle(stoi(xhist[2]));

	string date_time = "";
	date_time.append(xhist[3]);
	date_time.append(" ");
	date_time.append(xhist[4]);

	result.set_date_time(date_time);

	string date_time_end = "";
	date_time_end.append(xhist[5]);
	date_time_end.append(" ");
	date_time_end.append(xhist[6]);

	result.set_date_time(date_time_end, false);

	bool is_positived = xhist[7] == "P";
	bool is_cancelled = xhist[7] == "C";

	result.set_result(is_positived, is_cancelled);

	result.set_temperature(stoi(xhist[8]));
}

void print_ampoule_test_history_temp(string temp) {
	AmpouleTestResult result = AmpouleTestResult();

	to_ampoule_test_result(temp, result);

	print_ampoule_test_history(result);
}

void print_test_task_notify(void *pvParameter) {
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(500);
	BaseType_t xResult;

	uint32_t ulNotifiedValue;

	for (;;) {
		/* Wait to be notified of an interrupt. */
		xResult = xTaskNotifyWait(pdFALSE, /* Don't clear bits on entry. */
		ULONG_MAX, /* Clear all bits on exit. */
		&ulNotifiedValue, /* Stores the notified value. */
		xMaxBlockTime);

		if (xResult == pdPASS) {
			if (usb_print_setup_done()) {

				uint8_t print_count = get_print_count();
				int total = ampoule_history_get_count();

				if (print_count > total)
					print_count = total;

				// Imprime do mais antigo para o mais recente dentro do
				// recorte escolhido, para o ticket sair na ordem
				// cronologica na impressora.
				for (int i = print_count - 1; i >= 0; i--) {
					ampoule_history_record_t rec;

					if (!ampoule_history_get_record(i, rec))
						continue;

					AmpouleTestResult result = AmpouleTestResult();

					ampoule_history_record_to_result(rec, result);

					print_ampoule_test_history(result);
				}

//				for (int i = 0; i < 4; i++) {
//					if (is_test_done(i + 1)) {
//						print_ampoule_test(i);
//					}
//				}

			}
		}

		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void printer_setup() {
	xTaskCreate(print_test_task_notify, "PRINT_TST",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 9, &print_ampoule_test_task_handle);

//	printer_daemon_setup();
//
	xTaskCreate(print_check_status, "PRINT_CHECK",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 10, NULL);
}

string get_partner_name() {
	return branding_get(LOGO)->partner_upper;
}

string get_partner_name_formatted(){
	return branding_get(LOGO)->partner_formatted;
}

string get_type_test(){
	return branding_get(LOGO)->type_test;
}

void print_test_cancelled(string ampola, string id_test, string dt_inicio,
		string hr_inicio, string ciclo, int temperature, string serial_number,
		string inst, string tempo_em_teste, string language) {
	TPrinter printer = TPrinter();

	printer.begin();

	printer.Separator('-');
	printer.newLine();

	printer.justify('C');

	// printer.TextSize(60);
	// printer.append("LUMMINA 4");
	// printer.TextSize(0);
	// printer.newLine();

	printer.TextSize( FontWidth::DoubleWidth2);
	printer.append("LUMMINA 4");
	printer.TextSize( FontWidth::Normal);
	
	printer.newLine();
	printer.newLine();

	if (language == "pt-br") {

		printer.append("INCUBADORA BIOL");
		printer.appendByte(0x9F); // Ã“
		printer.append("GICA", true);

		printer.append("N/S: ");
	} else if (language == "en-us") {
		printer.append("BIOLOGICAL INCUBATOR", true);

		printer.append("S/N: ");
	} else {
		printer.append("INCUBADORA BIOL");
		printer.appendByte(0x9F); // Ã“
		printer.append("GICA", true);

		printer.append("N/S: ");
	}

	printer.append(serial_number.c_str(), true);

	printer.newLine();

	printer.append(inst.c_str(), true);

	printer.Separator('-');
	printer.newLine();

	printer.justify('L');

	if (language == "pt-br") {
		printer.append("IB - ", false);
	} else if (language == "en-us") {
		printer.append("BI - ", false);
	} else {
		printer.append("IB - ", false);
	}

	printer.append(get_partner_name_formatted().c_str(), true);

	if (language == "pt-br") {
		printer.append("TIPO: ");
	} else if (language == "en-us") {
		printer.append("TYPE: ");
	} else {
		printer.append("TIPO: ");
	}

	printer.append(get_type_test().c_str());
	printer.append(" ");

	printer.append(ciclo.c_str());
	printer.append(" - ");
	printer.append(to_string(temperature).c_str());
	printer.appendByte(0xF8);
	printer.append("C", true);

	if (language == "pt-br") {
		printer.append("CAVIDADE: ");
	} else if (language == "en-us") {
		printer.append("CAVITY: ");
	} else {
		printer.append("CAVIDAD: ");
	}

	printer.append(ampola.c_str(), true);

	if (language == "pt-br") {
		printer.append("TEMPO DE LEITURA: ");
	} else if (language == "en-us") {
		printer.append("READING TIME: ");
	} else {
		printer.append("TIEMPO DE LEER: ");
	}

	printer.append(tempo_em_teste.c_str(), true);

	printer.newLine();
	printer.newLine();

	printer.justify('C');

	//printer.TextSize(20);

	printer.TextSize( FontWidth::DoubleWidth2);
	//printer.append("LUMMINA 4");
	//printer.newLine();


	if (language == "pt-br") {
		printer.Bold("** CANCELADO **");
	} else if (language == "en-us") {
		printer.Bold("** CANCELED **");
	} else {
		printer.Bold("** CANCELADO **");
	}

	printer.TextSize( FontWidth::Normal);

	//printer.TextSize(0);
	printer.newLine();
	printer.newLine();
	printer.newLine();
	printer.newLine();

	printer.justify('L');

	if (language == "pt-br") {
		printer.append("N");
		printer.appendByte(0xF8);
		printer.append(" INCUB.:");
	} else if (language == "en-us") {
		printer.append("INCUB. NUM:");
	} else {
		printer.append("N");
		printer.appendByte(0xF8);
		printer.append(" INCUB.:");
	}

	printer.append(id_test.c_str(), true);

	if (language == "pt-br") {
		printer.append("DATA:");
	} else if (language == "en-us") {
		printer.append("DATE:");
	} else {
		printer.append("FECHA:");
	}

	printer.append(dt_inicio.c_str());

	if (language == "pt-br") {
		printer.append(" HORA:");
	} else if (language == "en-us") {
		printer.append(" HOUR:");
	} else {
		printer.append(" HORA:");
	}

	printer.append(hr_inicio.c_str());

	printer.newLine();
	printer.newLine();

	printer.justify('C');

	printer.append(get_partner_name().c_str(), true);

	printer.Separator('*');

	printer.newLine();
	printer.newLine();
	printer.newLine();
	printer.newLine();
	//printer.feedLine(2);

	printer.end();

	vTaskDelay(pdMS_TO_TICKS(500));

	usb_print(printer.print(), printer.size(), true);

	printer.clear();
}

void print_test(string ampola, string id_test, string dt_inicio,
		string hr_inicio, string dt_fim, string hr_fim, int resultado,
		string ciclo, int temperature, string serial_number, string inst,
		string tempo_em_teste, string language) {
	TPrinter printer = TPrinter();

	printer.begin();

	printer.Separator('-');
	printer.newLine();

	printer.justify('C');

	// printer.TextSize(60);
	// printer.append("LUMMINA 4");
	// printer.TextSize(0);
	// printer.newLine();

	printer.TextSize( FontWidth::DoubleWidth2);
	printer.append("LUMMINA 4");
	printer.TextSize( FontWidth::Normal);
	
	printer.newLine();
	printer.newLine();

	if (language == "pt-br") {

		printer.append("INCUBADORA BIOL");
		printer.appendByte(0x9F); // Ã“
		printer.append("GICA", true);

		printer.append("N/S: ");
	} else if (language == "en-us") {
		printer.append("BIOLOGICAL INCUBATOR", true);

		printer.append("S/N: ");
	} else {
		printer.append("INCUBADORA BIOL");
		printer.appendByte(0x9F); // Ã“
		printer.append("GICA", true);

		printer.append("N/S: ");
	}

	printer.append(serial_number.c_str(), true);

	printer.newLine();

	printer.append(inst.c_str(), true);

	printer.Separator('-');
	printer.newLine();

	printer.justify('L');

	if (language == "pt-br") {
		printer.append("IB - ", false);
	} else if (language == "en-us") {
		printer.append("BI - ", false);
	} else {
		printer.append("IB - ", false);
	}

	printer.append(get_partner_name_formatted().c_str(), true);

	if (language == "pt-br") {
		printer.append("TIPO: ");
	} else if (language == "en-us") {
		printer.append("TYPE: ");
	} else {
		printer.append("TIPO: ");
	}

	printer.append(get_type_test().c_str());
	printer.append(" ");

	printer.append(ciclo.c_str());
	printer.append(" - ");
	printer.append(to_string(temperature).c_str());
	printer.appendByte(0xF8);
	printer.append("C", true);

	if (language == "pt-br") {
		printer.append("CAVIDADE: ");
	} else if (language == "en-us") {
		printer.append("CAVITY: ");
	} else {
		printer.append("CAVIDAD: ");
	}

	printer.append(ampola.c_str(), true);

	if (language == "pt-br") {
		printer.append("TEMPO DE LEITURA: ");
	} else if (language == "en-us") {
		printer.append("READING TIME: ");
	} else {
		printer.append("TIEMPO DE LEER: ");
	}

	printer.append(tempo_em_teste.c_str(), true);

	printer.newLine();
	printer.newLine();

	printer.justify('C');

	printer.TextSize( FontWidth::DoubleWidth2);

	if (resultado == 1) {
		if (language == "pt-br") {
			printer.Bold("* POSITIVO *");
		} else if (language == "en-us") {
			printer.Bold("* POSITIVE *");
		} else {
			printer.Bold("* POSITIVO *");
		}
	} else {
		if (language == "pt-br") {
			printer.Bold("* NEGATIVO *");
		} else if (language == "en-us") {
			printer.Bold("* NEGATIVE *");
		} else {
			printer.Bold("* NEGATIVO *");
		}
	}

	printer.TextSize( FontWidth::Normal);
	printer.newLine();

	printer.newLine();
	printer.newLine();

	printer.justify('L');

	if (language == "pt-br") {
		printer.append("N");
		printer.appendByte(0xF8);
		printer.append(" INCUB.:");
	} else if (language == "en-us") {
		printer.append("INCUB. NUM:");
	} else {
		printer.append("N");
		printer.appendByte(0xF8);
		printer.append(" INCUB.:");
	}

	printer.append(id_test.c_str(), true);

	if (language == "pt-br") {
		printer.append("DATA:");
	} else if (language == "en-us") {
		printer.append("DATE:");
	} else {
		printer.append("FECHA:");
	}

	printer.append(dt_inicio.c_str());

	if (language == "pt-br") {
		printer.append(" HORA:");
	} else if (language == "en-us") {
		printer.append(" HOUR:");
	} else {
		printer.append(" HORA:");
	}

	printer.append(hr_inicio.c_str());

	printer.newLine();
	printer.newLine();

	printer.justify('C');

	printer.append(get_partner_name().c_str(), true);

	printer.Separator('*');

	printer.newLine();
	printer.newLine();
	printer.newLine();
	printer.newLine();

	printer.end();

	vTaskDelay(pdMS_TO_TICKS(500));

	usb_print(printer.print(), printer.size(), true);

	printer.clear();
}