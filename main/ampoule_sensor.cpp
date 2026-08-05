#include "include/ampoule_sensor.h"
#include "include/pcf8574_driver.h"
#include "include/task_manager.h"
#include "include/buzzer.h"
#include "include/ampoule_test.h"
#include "esp_timer.h"

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

// Numero de leituras consecutivas (a cada 300ms) que precisam concordar antes
// de aceitar uma mudanca de presenca de ampola. Sem isso, uma unica leitura
// ruidosa do I2C (barramento ja documentado como instavel) derruba
// is_present, e ampoule_test() para de processar aquela cavidade em
// silencio, pra sempre, ate a proxima leitura "correta" por acaso.
#define AMPOULE_PRESENCE_DEBOUNCE_COUNT 4

// Numero de leituras rapidas feitas por pino em cada ciclo, e quantas
// precisam concordar para aceitar o valor "bruto" daquele ciclo (filtra
// ruido eletrico pontual de uma unica transacao I2C corrompida). Com um
// sinal binario, 5 amostras so podem se dividir 5-0, 4-1 ou 3-2 - exigir
// "mais de 2 iguais" (>= 3 de 5) garante sempre um resultado decisivo;
// exigir >= 4 de 5 deixaria o caso 3-2 sem decisao.
#define AMPOULE_FAST_SAMPLE_COUNT 5
#define AMPOULE_FAST_SAMPLE_MAJORITY 3

// Tempo continuo sem nenhuma ampola detectada, exigido antes de liberar a
// trava de boot (is_ampoules_present_in_init). Qualquer deteccao de
// presenca durante essa janela zera o contador - so libera depois de 10s
// "limpos" seguidos. Substitui a checagem de uma unica leitura confirmada,
// que se mostrou fragil em teste fisico (05/08): a trava liberava cedo
// demais mesmo com ampola fisicamente presente na cavidade ao ligar.
#define AMPOULE_ABSENT_CONFIRM_MS (10 * 1000)

static bool read_pin_majority(uint8_t pin) {
	int present_count = 0;

	for (int i = 0; i < AMPOULE_FAST_SAMPLE_COUNT; i++) {
		if (ampoule.digital_read(pin) == 0)
			present_count++;
	}

	return present_count >= AMPOULE_FAST_SAMPLE_MAJORITY;
}

void read_ampoules(void *pvParameter) {

	bool confirmed_present[4] = { false, false, false, false };
	uint8_t debounce_count[4] = { 0, 0, 0, 0 };
	int64_t no_ampoule_since_ms = 0;

	while (1) {

		bool raw_present[4] = { read_pin_majority(P4), read_pin_majority(P5),
				read_pin_majority(P6), read_pin_majority(P7) };

		for (int i = 0; i < 4; i++) {
			if (raw_present[i] == confirmed_present[i]) {
				debounce_count[i] = 0;
			} else {
				debounce_count[i]++;

				if (debounce_count[i] >= AMPOULE_PRESENCE_DEBOUNCE_COUNT) {
					confirmed_present[i] = raw_present[i];
					debounce_count[i] = 0;
				}
			}
		}

		bool is_ampoules = (confirmed_present[0] || confirmed_present[1]
				|| confirmed_present[2] || confirmed_present[3]);

		if (is_ampoules_present_in_init) {
			if (is_ampoules) {
				no_ampoule_since_ms = 0;
			} else {
				int64_t now_ms = esp_timer_get_time() / 1000;

				if (no_ampoule_since_ms == 0) {
					no_ampoule_since_ms = now_ms;
				} else if (now_ms - no_ampoule_since_ms
						>= AMPOULE_ABSENT_CONFIRM_MS) {
					is_ampoules_present_in_init = false;
				}
			}
		}

		if (!is_ampoules_present_in_init) {
			ampoule_set_status(confirmed_present[0], confirmed_present[1],
					confirmed_present[2], confirmed_present[3]);
		}

		vTaskDelay(pdMS_TO_TICKS(300));
	}
}

void ampoule_sensor_main() {
	xTaskCreate(read_ampoules, "READ_AMPOULES",
	configMINIMAL_STACK_SIZE * 5,
	NULL, 5, NULL);
}
