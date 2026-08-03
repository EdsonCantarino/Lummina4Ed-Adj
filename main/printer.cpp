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
#include "esp_timer.h"
#include "esp_system.h"

using namespace std;

static const char *TAG = "PRINTER";

device_settings_t settings;

// Watchdog de impressora travada/desconectada. Duas correcoes de USB em
// runtime (re-registrar client, e depois reinstalar a lib USB inteira)
// se mostraram instaveis em teste fisico (3 formas diferentes de
// crash/deadlock) - a recuperacao confiavel e um esp_restart(), mas so
// quando nenhuma cavidade estiver testando (ver attempt_safe_printer_recovery),
// pra nunca arriscar perder um teste em andamento.
#define PRINT_HANG_TIMEOUT_MS (15 * 1000)
#define PRINTER_DISCONNECTED_TIMEOUT_MS (30 * 1000)

static volatile int64_t print_operation_started_ms = 0;
static int64_t printer_disconnected_since_ms = 0;
static bool printer_ever_connected = false;
static bool printer_was_ready = false;

// Bits usados em xTaskNotify(print_ampoule_test_task_handle, ...): botao
// fisico de imprimir reimprime os ultimos N (print_count); a reconexao da
// impressora dispara so os pendentes (nao impressos ainda) - ver
// print_test_task_notify() e print_pending_unprinted_history().
#define PRINT_NOTIFY_BUTTON         (1 << 0)
#define PRINT_NOTIFY_PENDING_REPRINT (1 << 1)

static void print_operation_begin() {
	print_operation_started_ms = esp_timer_get_time() / 1000;
	set_is_priting(true);
}

// Retorna se a impressao provavelmente teve sucesso. O envio USB e
// assincrono - o pequeno delay da tempo da callback (transfer_cb em
// usb_class_driver.cpp) atualizar isPrinterError/isSetupDone antes da
// gente checar.
static bool print_operation_end() {
	set_is_priting(false);
	print_operation_started_ms = 0;

	vTaskDelay(pdMS_TO_TICKS(300));

	return is_printer_connected() && !is_printer_error();
}

static bool any_cavity_testing() {
	return is_testing(1) || is_testing(2) || is_testing(3) || is_testing(4);
}

// So reinicia se nenhuma cavidade estiver testando - senao so loga e
// tenta de novo no proximo ciclo do watchdog (1s).
static void attempt_safe_printer_recovery(const char *reason) {
	if (any_cavity_testing()) {
		ESP_LOGW(TAG,
				"%s, mas ha cavidade em teste - adiando reinicio ate ficar seguro",
				reason);
		return;
	}

	ESP_LOGE(TAG, "%s - reiniciando o equipamento pra recuperar a impressora",
			reason);
	esp_restart();
}

void print_test(string ampola, string id_test, string dt_inicio,
		string hr_inicio, string dt_fim, string hr_fim, int resultado,
		string ciclo, int temperature, string serial_number, string inst,
		string tempo_em_teste, string language);

void print_test_cancelled(string ampola, string id_test, string dt_inicio,
		string hr_inicio, string ciclo, int temperature, string serial_number,
		string inst, string tempo_em_teste, string language);

// Ticket imprime a hora sem os segundos; hour_start/hour_end seguem com
// segundos internamente (usados no calculo de duracao do teste).
static string strip_seconds(const string &hhmmss) {
	return hhmmss.size() >= 5 ? hhmmss.substr(0, 5) : hhmmss;
}

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

		if (print_operation_started_ms > 0) {
			int64_t elapsed = (esp_timer_get_time() / 1000)
					- print_operation_started_ms;

			if (elapsed >= PRINT_HANG_TIMEOUT_MS) {
				print_operation_started_ms = 0;
				set_is_priting(false);

				char reason[64];
				snprintf(reason, sizeof(reason),
						"Impressao travada ha %lld ms sem concluir",
						(long long) elapsed);
				attempt_safe_printer_recovery(reason);
			}
		}

		if (!is_printer_connected()) {
			print_leds(1);
			printer_was_ready = false;

			// So considera "desconectada" pra fins de watchdog depois de ja
			// ter conectado uma vez - evita disparar um restart no boot,
			// antes da impressora terminar a enumeracao USB normal.
			if (printer_ever_connected) {
				if (printer_disconnected_since_ms == 0) {
					printer_disconnected_since_ms = esp_timer_get_time()
							/ 1000;
				} else {
					int64_t disconnected_elapsed = (esp_timer_get_time()
							/ 1000) - printer_disconnected_since_ms;

					if (disconnected_elapsed
							>= PRINTER_DISCONNECTED_TIMEOUT_MS) {
						printer_disconnected_since_ms = 0;

						char reason[64];
						snprintf(reason, sizeof(reason),
								"Impressora desconectada ha %lld ms",
								(long long) disconnected_elapsed);
						attempt_safe_printer_recovery(reason);
					}
				}
			}
		} else {
			printer_ever_connected = true;
			printer_disconnected_since_ms = 0;

			statePrinterConnected = is_setup_done() && !is_printer_error();
			print_leds(!statePrinterConnected);

			// Borda desconectada->pronta: dispara reimpressao dos tickets
			// pendentes (nao impressos por erro/desconexao anterior).
			if (statePrinterConnected && !printer_was_ready
					&& print_ampoule_test_task_handle != NULL) {
				xTaskNotify(print_ampoule_test_task_handle,
				PRINT_NOTIFY_PENDING_REPRINT, eSetBits);
			}
			printer_was_ready = statePrinterConnected;

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

void print_ampoule_test_history(AmpouleTestResult &amp, uint32_t id_test = 0) {
	print_operation_begin();

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

	bool ok = print_operation_end();

	if (ok && id_test != 0) {
		ampoule_history_mark_printed(id_test);
	}
}

void print_history() {

}

bool print_ampoule_test(int id, bool is_cancelled = false) {

	string institution = get_institution();
	string serial_number = get_serial_number();
	string language = get_language();

	AmpouleTestResult amp = get_test_result(id);

	ESP_LOGI("", "Start Date & Time Formatted: %s",
			amp.get_formated_date_time(true, " | ").c_str());
	ESP_LOGI("", "End   Date & Time Formatted: %s",
			amp.get_formated_date_time(false, " | ").c_str());

	print_operation_begin();

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

	return print_operation_end();
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

// Reimprime so os tickets pendentes (nao impressos com sucesso ainda),
// varrendo do mais recente pro mais antigo e parando no primeiro ja
// impresso - nao pula pra procurar nao-impressos alem dele (ex: historico
// de antes dessa funcionalidade existir, ou intervalo ja tratado de outra
// forma). Chamado quando a impressora volta a ficar pronta (ver
// print_check_status).
static void print_pending_unprinted_history() {
	int total = ampoule_history_get_count();

	int first_unprinted = -1;
	int last_unprinted = -1;

	for (int i = 0; i < total; i++) {
		ampoule_history_record_t rec;

		if (!ampoule_history_get_record(i, rec))
			break;

		if (rec.printed)
			break;

		if (first_unprinted < 0)
			first_unprinted = i;
		last_unprinted = i;
	}

	if (first_unprinted < 0)
		return;

	ESP_LOGW(TAG,
			"Reimprimindo %d ticket(s) pendente(s) apos a impressora ficar pronta",
			last_unprinted - first_unprinted + 1);

	// Imprime do mais antigo pro mais recente (ordem cronologica no papel).
	for (int i = last_unprinted; i >= first_unprinted; i--) {
		ampoule_history_record_t rec;

		if (!ampoule_history_get_record(i, rec))
			continue;

		AmpouleTestResult result = AmpouleTestResult();

		ampoule_history_record_to_result(rec, result);

		print_ampoule_test_history(result, rec.id_test);
	}
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

				if (ulNotifiedValue & PRINT_NOTIFY_BUTTON) {
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

						print_ampoule_test_history(result, rec.id_test);
					}
				}

				if (ulNotifiedValue & PRINT_NOTIFY_PENDING_REPRINT) {
					print_pending_unprinted_history();
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

	printer.append(strip_seconds(hr_inicio).c_str());

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

	printer.append(strip_seconds(hr_inicio).c_str());

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