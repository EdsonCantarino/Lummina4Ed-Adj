#include "include/light_sensor.h"
#include "esp_system.h"
#include "sdkconfig.h"

// Driver do ADS1248 (TI) - placa nova, ainda em bring-up. So compilado quando
// selecionado em "ADC (conversor analogico)" no menuconfig. Implementa a
// mesma API publica de light_sensor.h que o driver do CS5534
// (light_sensor.cpp) ja implementa hoje, entao ampoule_test.cpp e o resto do
// firmware nao precisam saber qual chip esta fisicamente na placa.
//
// Bit-bang em GPIO, igual o CS5534 - de proposito, nao spi_master do
// ESP-IDF. Ja foi tentado usar spi_master (ver components/cs5534/cs553x.cpp,
// que ficou todo comentado) e deu problema, motivo nao documentado na epoca.
// TODO(pos-validacao): reavaliar migrar pra spi_master depois que esse driver
// estiver validado fisicamente em producao - nao e bloqueante agora.
#if CONFIG_ADC_CHIP_ADS1248

#include "ads1248.h"

static const char *TAG = "LIGHT_SENSOR_ADS1248";

#define HIGH 1
#define LOW 0

// Mesmos GPIOs que o CS5534 ja usa hoje pro barramento SPI (a placa nova
// reaproveitou o roteamento de proposito). Os tres pinos dedicados abaixo
// (DRDY, RESET, START) nao existem no CS5534.
#define ADS_SCLK_PIN  (gpio_num_t)12
#define ADS_CS_PIN    (gpio_num_t)10
#define ADS_DIN_PIN   (gpio_num_t)13 // ESP32 -> ADS1248 (DIN do chip)
#define ADS_DOUT_PIN  (gpio_num_t)11 // ADS1248 -> ESP32 (DOUT/DRDY do chip, so DOUT aqui)
#define ADS_DRDY_PIN  (gpio_num_t)39 // DRDY dedicado (ADS1248 -> ESP32)
#define ADS_RESET_PIN (gpio_num_t)4  // ~RESET~ (ESP32 -> ADS1248)
#define ADS_START_PIN (gpio_num_t)40 // START (ESP32 -> ADS1248)

// Timeout de espera pelo DRDY do ADS1248, mesmo criterio de folga usado no
// driver do CS5534 (ver CS5532_DRDY_TIMEOUT_MS em light_sensor.cpp).
#define ADS1248_DRDY_TIMEOUT_MS 1000

#define ADS1248_BITBANG_DELAY 200

// Mapa cavidade (indice de software 0-3) -> par de entrada diferencial do
// ADS1248, confirmado pelo netlist da placa nova (Versao ADS1428/Lummina4
// ADS1248.net) e pelos pinos fisicos do chip: cavidade1/SENSOR1 = AIN2/AIN3,
// cavidade2/SENSOR2 = AIN6/AIN7, cavidade3/SENSOR3 = AIN0/AIN1,
// cavidade4/SENSOR4 = AIN4/AIN5. Nao e sequencial por causa do roteamento
// fisico da placa - ver historico da sessao que adicionou esse driver.
static const uint8_t channel_mux_sp[4] = { ADS1248_AIN2, ADS1248_AIN6,
		ADS1248_AIN0, ADS1248_AIN4 };
static const uint8_t channel_mux_sn[4] = { ADS1248_AIN3, ADS1248_AIN7,
		ADS1248_AIN1, ADS1248_AIN5 };

// Ultima leitura valida e contador de falhas consecutivas por canal (0-3) -
// mesmo padrao do driver do CS5534 (ver last_valid_channel_value /
// channel_consecutive_timeouts em light_sensor.cpp).
static long last_valid_channel_value[4] = { 0, 0, 0, 0 };
static uint8_t channel_consecutive_timeouts[4] = { 0, 0, 0, 0 };

uint8_t get_channel_consecutive_timeouts(uint8_t channel) {
	if (channel > 3)
		return 0;
	return channel_consecutive_timeouts[channel];
}

void reset_channel_consecutive_timeouts(uint8_t channel) {
	if (channel > 3)
		return;
	channel_consecutive_timeouts[channel] = 0;
}

static void ads1248_delay(unsigned long vDelay) {
	for (; vDelay != 0; vDelay--)
		;
}

static void ads1248_cs_low(void) {
	gpio_set_level(ADS_SCLK_PIN, LOW);
	gpio_set_level(ADS_CS_PIN, LOW);
	ads1248_delay(ADS1248_BITBANG_DELAY);
}

static void ads1248_cs_high(void) {
	gpio_set_level(ADS_SCLK_PIN, LOW);
	gpio_set_level(ADS_CS_PIN, HIGH);
	ads1248_delay(ADS1248_BITBANG_DELAY);
}

// Um byte por vez, MSB primeiro. Datasheet secao 9.5.1.2/9.5.1.3: o ADS1248
// le o DIN na borda de DESCIDA do SCLK e atualiza o DOUT na borda de SUBIDA -
// e o oposto do jeito que o CS5534 bit-banga hoje, entao NAO reaproveitar a
// logica de borda do CS5534 aqui.
static uint8_t ads1248_byte(uint8_t out) {
	uint8_t in = 0;
	for (int i = 7; i >= 0; i--) {
		gpio_set_level(ADS_DIN_PIN, (out >> i) & 0x01);
		ads1248_delay(ADS1248_BITBANG_DELAY);

		gpio_set_level(ADS_SCLK_PIN, HIGH); // DOUT valido apos essa borda
		ads1248_delay(ADS1248_BITBANG_DELAY);
		in = (uint8_t) ((in << 1) | (gpio_get_level(ADS_DOUT_PIN) & 0x01));

		gpio_set_level(ADS_SCLK_PIN, LOW); // ADS1248 le o DIN nessa borda
		ads1248_delay(ADS1248_BITBANG_DELAY);
	}
	return in;
}

static void ads1248_wreg(uint8_t reg_addr, uint8_t value) {
	ads1248_cs_low();
	ads1248_byte((uint8_t) (ADS1248_CMD_WREG | (reg_addr & 0x0F)));
	ads1248_byte(0x00); // numero de registradores a escrever menos 1 (so 1)
	ads1248_byte(value);
	ads1248_cs_high();
}

static uint8_t ads1248_rreg(uint8_t reg_addr) {
	ads1248_cs_low();
	ads1248_byte((uint8_t) (ADS1248_CMD_RREG | (reg_addr & 0x0F)));
	ads1248_byte(0x00); // numero de registradores a ler menos 1 (so 1)
	uint8_t value = ads1248_byte(ADS1248_CMD_NOP);
	ads1248_cs_high();
	return value;
}

static void ads1248_command(uint8_t cmd) {
	ads1248_cs_low();
	ads1248_byte(cmd);
	ads1248_cs_high();
}

// Manda o comando RDATA e le os 24 bits do resultado da conversao, com
// sign-extend de complemento-de-2 pra um long de 32 bits.
static long ads1248_rdata(void) {
	ads1248_cs_low();
	ads1248_byte(ADS1248_CMD_RDATA);

	uint32_t raw24 = 0;
	for (int i = 0; i < 3; i++) {
		raw24 = (raw24 << 8) | ads1248_byte(ADS1248_CMD_NOP);
	}
	ads1248_cs_high();

	if (raw24 & 0x800000UL) {
		return (long) (raw24 | 0xFF000000UL);
	}
	return (long) raw24;
}

static void setup_pins() {
	gpio_config_t out_conf = { };
	out_conf.pull_down_en = (gpio_pulldown_t) 0;
	out_conf.pull_up_en = (gpio_pullup_t) 0;
	out_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	out_conf.mode = GPIO_MODE_OUTPUT;
	out_conf.pin_bit_mask = (1ULL << ADS_SCLK_PIN) | (1ULL << ADS_CS_PIN)
			| (1ULL << ADS_DIN_PIN) | (1ULL << ADS_RESET_PIN)
			| (1ULL << ADS_START_PIN);
	gpio_config(&out_conf);

	gpio_config_t in_conf = { };
	in_conf.pull_down_en = (gpio_pulldown_t) 0;
	in_conf.pull_up_en = (gpio_pullup_t) 0;
	in_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	in_conf.mode = GPIO_MODE_INPUT;
	in_conf.pin_bit_mask = (1ULL << ADS_DOUT_PIN) | (1ULL << ADS_DRDY_PIN);
	gpio_config(&in_conf);

	gpio_set_level(ADS_SCLK_PIN, LOW);
	gpio_set_level(ADS_CS_PIN, HIGH);
	gpio_set_level(ADS_DIN_PIN, LOW);
	gpio_set_level(ADS_RESET_PIN, HIGH);
	gpio_set_level(ADS_START_PIN, LOW);
}

void light_sensor_setup() {
	setup_pins();

	// Reset via pino dedicado (o ADS1248 tem RESET fisico, diferente do
	// CS5534 que so reseta por comando/registrador). Datasheet Figura 4:
	// RESET baixo >= 4 tCLK, depois espera >= 0.6ms antes de comunicar.
	gpio_set_level(ADS_RESET_PIN, LOW);
	vTaskDelay(pdMS_TO_TICKS(1));
	gpio_set_level(ADS_RESET_PIN, HIGH);
	vTaskDelay(pdMS_TO_TICKS(1));

	// START precisa estar alto antes de qualquer WREG - com START baixo o
	// ADS1248 esta em poder-down e so aceita RDATA/RDATAC/SDATAC/WAKEUP/NOP
	// (datasheet secao 9.5.1.7).
	gpio_set_level(ADS_START_PIN, HIGH);
	vTaskDelay(pdMS_TO_TICKS(1));

	ads1248_command(ADS1248_CMD_SDATAC);

	// Referencia interna sempre ligada (necessaria pra qualquer leitura) e
	// entrada de referencia padrao REFP0/REFN0.
	ads1248_wreg(ADS1248_REG_MUX1,
			ADS1248_VREFCON_ALWAYS_ON | ADS1248_REFSELT_REFP0N0
					| ADS1248_MUXCAL_NORMAL);

	// Ponto de partida conservador (ganho 1, 20 SPS) - taxa/ganho definitivos
	// ficam pra depois da comparacao fisica com o CS5534 (ver plano da
	// sessao que adicionou esse driver).
	ads1248_wreg(ADS1248_REG_SYS0, ADS1248_PGA_1 | ADS1248_DR_20SPS);

	// Sem corrente de excitacao - sensor de luz nao usa IDAC.
	ads1248_wreg(ADS1248_REG_IDAC0,
			ADS1248_DRDY_MODE_DOUT_ONLY | ADS1248_IMAG_OFF);

	// Sanity check de fiacao SPI: le de volta o nibble de ID do IDAC0 (bits
	// 7:4, so leitura, dependente da revisao de silicio) - so log, nao trava
	// o boot se vier zero/inesperado.
	uint8_t idac0_readback = ads1248_rreg(ADS1248_REG_IDAC0);
	ESP_LOGI(TAG, "ADS1248 IDAC0 readback=0x%02X (ID nibble=0x%X)",
			idac0_readback, (idac0_readback >> 4) & 0x0F);
}

long read_channel_value(uint8_t channel) {
	if (channel > 3)
		return 0;

	// Grava MUX0 com o par AIN correto pra essa cavidade. Isso ja reseta o
	// filtro digital sozinho e reinicia a conversao (datasheet secao
	// 9.4.4.4), entao nao precisa de SYNC separado aqui.
	ads1248_wreg(ADS1248_REG_MUX0,
			ADS1248_MUX0_BYTE(channel_mux_sp[channel],
					channel_mux_sn[channel]));

	uint16_t drdy_wait_ms = 0;
	while (gpio_get_level(ADS_DRDY_PIN)) {
		vTaskDelay(pdMS_TO_TICKS(10));
		drdy_wait_ms += 10;

		if (drdy_wait_ms >= ADS1248_DRDY_TIMEOUT_MS) {
			ESP_LOGE(TAG,
					"Timeout aguardando DRDY do ADS1248 (canal %d) - assumindo ultima leitura valida",
					channel + 1);

			if (channel_consecutive_timeouts[channel] < 255)
				channel_consecutive_timeouts[channel]++;

			return last_valid_channel_value[channel];
		}
	}

	long raw = ads1248_rdata();

	last_valid_channel_value[channel] = raw;
	channel_consecutive_timeouts[channel] = 0;

	printf("Read CH%02d = %ld\n", channel + 1, raw);
#if CONFIG_ADC_DEBUG_SERIAL
	printf("[ADCDBG] chip=ADS1248 ch=%u raw=%ld\n", channel, raw);
#endif

	return raw;
}

void light_sensor_task(void *pvParameter) {
	while (1) {
		read_channel_value(0);
		read_channel_value(1);
		read_channel_value(2);
		read_channel_value(3);

		vTaskDelay(pdMS_TO_TICKS(500));
		printf("---------------------\r\n");
	}
}

void light_sensor_main() {
	xTaskCreate(light_sensor_task, "light_sensor", configMINIMAL_STACK_SIZE * 4,
	NULL, 5, NULL);
}

#endif // CONFIG_ADC_CHIP_ADS1248
