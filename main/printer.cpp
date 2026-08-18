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
// crash/deadlock) - a recuperacao confiavel e um esp_restart(), disparado
// direto pelo flag isPrinterError do driver USB (is_printer_error(),
// usb_class_driver.cpp - reflete falha real de transferencia USB). So
// reinicia se houver ticket pendente no historico esperando pra ser
// impresso (ver has_unprinted_history() - 07/08: bancada de teste sem
// impressora conectada ficava reiniciando em loop sem motivo, ja que
// nao havia nada pendente pra imprimir mesmo) - e alem disso, se nao
// houver ampola em nenhuma cavidade e nenhum
// cancelamento/finalizacao de teste em andamento (ver
// attempt_safe_printer_recovery + ampoule_finalize_in_progress,
// ampoule_test.cpp) - is_testing() nao serve de guarda porque vira false
// assim que o ciclo termina, antes do ticket ser impresso e antes do
// operador remover a ampola; ampoule_any() reflete presenca fisica real
// (sensor), so fica false depois da remocao de verdade - protege tambem o
// caso "impressora trava bem no fim do ciclo, ampola ainda la dentro"
// (relatado pelo cliente 05/08). Depois de ficar seguro, ainda exige
// PRINTER_RESTART_GRACE_MS continuos nesse estado antes de reiniciar de
// fato (contador zera a cada deteccao de ampola/finalizacao em andamento,
// mesmo padrao do AMPOULE_ABSENT_CONFIRM_MS em ampoule_sensor.cpp) - da
// tempo de qualquer beep/feedback de UI em andamento terminar de tocar
// (06/08: reinicio cortava o beep de alarme de ampola removida no meio).
static bool printer_was_ready = false;

// Foto tirada uma unica vez, ~10s depois do boot (resolve_boot_pending_history_task) -
// diferente do estado atual de is_printer_connected() (que vira false de
// novo assim que o cabo sai), essa fica fixa pro resto do boot. Testado
// fisicamente em 12/08: impressora conectada no boot, imprime, cabo
// retirado no meio de outro teste - o reinicio de recuperacao deve
// continuar valendo (a impressora "deveria" estar la, so sumiu), diferente
// do caso "nunca teve impressora nesse boot" (bring-up ADS1248 em bancada,
// 07/08), onde reiniciar nao ajuda em nada. Se a impressora so for
// conectada depois dessa janela dos 10s iniciais, essa foto fica false e o
// reinicio de recuperacao nao dispara pra ela nessa sessao - aceito por
// decisao do usuario (12/08).
static bool printer_present_at_boot = false;

#define PRINTER_RESTART_GRACE_MS (7 * 1000)
static int64_t printer_safe_to_restart_since_ms = 0;

// Bits usados em xTaskNotify(print_ampoule_test_task_handle, ...): botao
// fisico de imprimir reimprime os ultimos N (print_count); a reconexao da
// impressora dispara so os pendentes (nao impressos ainda) - ver
// print_test_task_notify() e print_pending_unprinted_history().
#define PRINT_NOTIFY_BUTTON         (1 << 0)
#define PRINT_NOTIFY_PENDING_REPRINT (1 << 1)

// Limite de reimpressao automatica de pendentes quando a impressora volta a
// ficar pronta (ex.: reset do ESP32) - mantem so os N mais recentes, evita
// imprimir uma pilha grande de tickets antigos de uma vez.
#define PENDING_REPRINT_MAX 4

static void print_operation_begin() {
	set_is_priting(true);
}

// Retorna se a impressao provavelmente teve sucesso. O envio USB e
// assincrono - o pequeno delay da tempo da callback (transfer_cb em
// usb_class_driver.cpp) atualizar isPrinterError/isSetupDone antes da
// gente checar.
static bool print_operation_end() {
	set_is_priting(false);

	vTaskDelay(pdMS_TO_TICKS(300));

	return is_printer_connected() && !is_printer_error();
}

// So faz sentido reiniciar pra recuperar a impressora se houver algum
// ticket pendente (nao impresso) esperando por ela - sem isso, o unico
// "estrago" de ficar sem imprimir e nenhum, entao reiniciar o equipamento
// seria disruption sem motivo (ex.: bancada de teste sem impressora
// conectada, reiniciando sozinha em loop). Mesma suposicao de
// print_pending_unprinted_history(): pendentes sempre ficam no topo
// (indice 0 = mais recente), entao so precisa olhar o primeiro registro.
static bool has_unprinted_history() {
	int total = ampoule_history_get_count();

	if (total == 0)
		return false;

	ampoule_history_record_t rec;

	if (!ampoule_history_get_record(0, rec))
		return false;

	return !rec.printed;
}

// So reinicia se nao houver ampola em nenhuma cavidade havera pelo menos
// PRINTER_RESTART_GRACE_MS continuos nesse estado - senao so loga e tenta
// de novo no proximo ciclo do watchdog (1s). Qualquer deteccao de ampola
// ou cancelamento/finalizacao em andamento zera o contador, igual ao
// AMPOULE_ABSENT_CONFIRM_MS de ampoule_sensor.cpp.
static void attempt_safe_printer_recovery(const char *reason) {
	if (!has_unprinted_history()) {
		ESP_LOGW(TAG,
				"%s, mas nao ha ticket pendente no historico - sem motivo pra reiniciar",
				reason);
		printer_safe_to_restart_since_ms = 0;
		return;
	}

	// Usa a foto tirada no boot (printer_present_at_boot), nao o estado
	// atual - ver comentario na declaracao da variavel. Reiniciar so ajuda
	// a recuperar uma impressora que devia estar la (esteve presente no
	// boot) e sumiu/travou - se nunca teve impressora nesse boot, reiniciar
	// nao resolve nada (bring-up ADS1248 em bancada, 07/08).
	if (!printer_present_at_boot) {
		ESP_LOGW(TAG,
				"%s, mas a impressora nao estava presente no boot - sem motivo pra reiniciar (nada fisico pra recuperar)",
				reason);
		printer_safe_to_restart_since_ms = 0;
		return;
	}

	if (ampoule_any() || is_ampoule_finalize_in_progress()) {
		ESP_LOGW(TAG,
				"%s, mas ha ampola em alguma cavidade ou um cancelamento/finalizacao em andamento - adiando reinicio ate ficar seguro",
				reason);
		printer_safe_to_restart_since_ms = 0;
		return;
	}

	int64_t now_ms = esp_timer_get_time() / 1000;

	if (printer_safe_to_restart_since_ms == 0) {
		printer_safe_to_restart_since_ms = now_ms;
	}

	int64_t elapsed_ms = now_ms - printer_safe_to_restart_since_ms;

	if (elapsed_ms < PRINTER_RESTART_GRACE_MS) {
		ESP_LOGW(TAG,
				"%s, seguro pra reiniciar mas aguardando carencia (%lld/%d ms sem ampola)",
				reason, (long long) elapsed_ms, PRINTER_RESTART_GRACE_MS);
		return;
	}

	ESP_LOGE(TAG,
			"RESTART_ID=1 - %s - reiniciando o equipamento pra recuperar a impressora",
			reason);
	fflush(stdout);
	vTaskDelay(pdMS_TO_TICKS(100));
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

// Formata duracao "HH:MM:SS" (saida de diffDateTime, ver get_time_in_test())
// como "HHHMMMin" (ex.: "00H00Min", "01H23Min") pro campo "Tempo de leitura"
// do ticket - diferente de strip_seconds(), que so tira os segundos de um
// horario de relogio (ex.: hora de inicio) e mantem o "HH:MM".
static string format_duration_hm(const string &hhmmss) {
	if (hhmmss.size() < 5)
		return hhmmss;

	return hhmmss.substr(0, 2) + "H" + hhmmss.substr(3, 2) + "Min";
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

		if (is_printer_error()) {
			attempt_safe_printer_recovery(
					"Erro de impressora detectado (is_printer_error)");
		}

		if (!is_printer_connected()) {
			print_leds(1);
			printer_was_ready = false;
		} else {
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

	// Se ha mais pendentes que o limite, imprime so os mais recentes
	// (ate PENDING_REPRINT_MAX) e marca os mais antigos que sobrarem
	// como impressos sem imprimir de fato - senao ficariam presos pra
	// sempre atras do indice 0 (que vai virar "impresso" daqui a pouco),
	// ja que a varredura acima para no primeiro registro ja impresso e
	// nunca mais chegaria neles.
	if (last_unprinted - first_unprinted + 1 > PENDING_REPRINT_MAX) {
		int new_last_unprinted = first_unprinted + PENDING_REPRINT_MAX - 1;

		for (int i = last_unprinted; i > new_last_unprinted; i--) {
			ampoule_history_record_t rec;

			if (!ampoule_history_get_record(i, rec))
				continue;

			ampoule_history_mark_printed(rec.id_test);
		}

		last_unprinted = new_last_unprinted;
	}

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

// Espera a impressora ter chance de enumerar via USB (se houver uma
// conectada) e resolve QUALQUER backlog de ticket pendente do historico uma
// unica vez no boot - mesmo caminho pra reset por software (esp_restart) e
// pra energizar pela fonte pela primeira vez, ja que os dois passam por
// app_main()/printer_setup(). Se a impressora ficou pronta a tempo, tenta
// imprimir de verdade; do contrario (ou se falhar mesmo assim), marca como
// impresso de qualquer forma - um ticket antigo parado aqui nao serve pra
// nada (a guarda printer_present_at_boot em attempt_safe_printer_recovery
// ja evita reiniciar por causa dele, mas ele ficaria marcado "nao impresso"
// no historico web pra sempre sem essa limpeza).
#define BOOT_PENDING_HISTORY_RESOLVE_DELAY_MS (10 * 1000)

static void resolve_boot_pending_history_task(void *pvParameter) {
	vTaskDelay(pdMS_TO_TICKS(BOOT_PENDING_HISTORY_RESOLVE_DELAY_MS));

	// Foto unica da presenca da impressora nesse boot - ver comentario na
	// declaracao de printer_present_at_boot. Tirada aqui (nao antes) pra
	// dar tempo real da enumeracao USB acontecer (~600ms depois do client
	// registrar, testado em 12/08), e sempre, mesmo sem ticket pendente
	// agora - um ticket pode ficar pendente mais tarde, nessa mesma sessao.
	printer_present_at_boot = is_printer_connected();
	ESP_LOGI(TAG, "Boot: impressora %s no boot",
			printer_present_at_boot ? "presente" : "ausente");

	if (!has_unprinted_history()) {
		ESP_LOGI(TAG, "Boot: nenhum ticket pendente no historico");
		vTaskDelete(NULL);
		return;
	}

	if (is_printer_connected() && is_setup_done() && !is_printer_error()) {
		ESP_LOGW(TAG,
				"Boot: impressora pronta - imprimindo ticket(s) pendente(s)");
		print_pending_unprinted_history();
	}

	// Garante que nada fica pendente, com ou sem sucesso na tentativa acima
	// (sem impressora, ou impressora presente mas print falhou).
	int total = ampoule_history_get_count();

	for (int i = 0; i < total; i++) {
		ampoule_history_record_t rec;

		if (!ampoule_history_get_record(i, rec))
			break;

		if (rec.printed)
			break;

		ESP_LOGW(TAG,
				"Boot: marcando ticket id_test=%lu como impresso sem confirmacao de impressao",
				(unsigned long) rec.id_test);

		ampoule_history_mark_printed(rec.id_test);
	}

	vTaskDelete(NULL);
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

	xTaskCreate(resolve_boot_pending_history_task, "PRINT_BOOT_RESOLVE",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, NULL);
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

	printer.append(format_duration_hm(tempo_em_teste).c_str(), true);

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

	printer.append(format_duration_hm(tempo_em_teste).c_str(), true);

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