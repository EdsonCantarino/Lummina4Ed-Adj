#include "include/light_sensor.h"
#include "esp_system.h"
#include "sdkconfig.h"

// Driver do ADS1248 (TI) - placa nova, ainda em bring-up. So compilado quando
// selecionado em "ADC (conversor analogico)" no menuconfig. Implementa a
// mesma API publica de light_sensor.h que o driver do CS5534
// (light_sensor.cpp) ja implementa hoje, entao ampoule_test.cpp e o resto do
// firmware nao precisam saber qual chip esta fisicamente na placa.
//
// Usa o periferico de SPI de hardware do ESP32-S3 (spi_master), nao
// bit-bang. Migrado em 11/08/2026 durante a investigacao do reboot
// intermitente RTC_SW_SYS_RST (ver historico/2026-08-10b e 2026-08-11):
// hipoteses de estouro de stack/corrupcao de heap foram descartadas por
// teste direto, sobrando como hipotese lider colisao de timing entre a
// espera ocupada do bit-bang e o atendimento de interrupcao de WiFi/USB
// Host. spi_master usa o periferico de hardware (sem busy-wait manual por
// bit), reduzindo bastante a janela de CPU ocupada por transferencia.
//
// Uma tentativa anterior de usar spi_master nesse projeto (ver
// components/cs5534/cs553x.cpp, comentado) foi abandonada porque o dev
// anterior tentou compartilhar uma unica config de SPI entre CS5534 e
// ADS1248 (chips com modo/timing SPI diferentes). Aqui nao ha esse
// problema: CS5534 e ADS1248 sao compilados em variantes de firmware
// separadas (CONFIG_ADC_CHIP_ADS1248), cada driver configura o
// spi_master com o modo especifico do seu proprio chip.
#if CONFIG_ADC_CHIP_ADS1248

#include "ads1248.h"
#include "driver/spi_master.h"
#include "rom/ets_sys.h"

static const char *TAG = "LIGHT_SENSOR_ADS1248";

#define HIGH 1
#define LOW 0

// Mesmos GPIOs que o CS5534 ja usa hoje pro barramento SPI (a placa nova
// reaproveitou o roteamento de proposito). Os tres pinos dedicados abaixo
// (DRDY, RESET, START) nao existem no CS5534 e continuam GPIO simples,
// fora do barramento SPI.
#define ADS_SCLK_PIN  (gpio_num_t)12
#define ADS_CS_PIN    (gpio_num_t)10
#define ADS_DIN_PIN   (gpio_num_t)13 // ESP32 -> ADS1248 (DIN do chip / MOSI)
#define ADS_DOUT_PIN  (gpio_num_t)11 // ADS1248 -> ESP32 (DOUT do chip / MISO)
#define ADS_DRDY_PIN  (gpio_num_t)39 // DRDY dedicado (ADS1248 -> ESP32)
#define ADS_RESET_PIN (gpio_num_t)4  // ~RESET~ (ESP32 -> ADS1248)
#define ADS_START_PIN (gpio_num_t)40 // START (ESP32 -> ADS1248)

// Timeout de espera pelo DRDY do ADS1248, mesmo criterio de folga usado no
// driver do CS5534 (ver CS5532_DRDY_TIMEOUT_MS em light_sensor.cpp).
#define ADS1248_DRDY_TIMEOUT_MS 1000

// Barramento de hardware dedicado ao ADS1248 - nada mais no projeto usa
// SPI2_HOST hoje (CS5534 bit-banga em GPIO puro, nao usa periferico SPI).
#define ADS_SPI_HOST SPI2_HOST

// Clock conservador (bem abaixo do maximo do chip) - prioridade aqui e
// robustez/margem de timing, nao velocidade maxima; o ADC roda a 20SPS
// mesmo, entao nao ha ganho pratico em ir mais rapido.
#define ADS1248_SPI_CLOCK_HZ (500 * 1000)

// Delay minimo entre o byte de comando RDATA e o inicio dos bytes de dado
// (datasheet: t6, alguns tCLK de folga) - so relevante pra RDATA porque e
// o unico comando que muda de "escrevendo comando" pra "lendo conversao"
// dentro da mesma janela de CS baixo. wreg/rreg/command sao continuos
// porque so tem comando+parametros, sem essa troca de fase.
#define ADS1248_RDATA_T6_DELAY_US 5

static spi_device_handle_t ads1248_spi;

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

// Transferencia full-duplex de "len" bytes numa unica transacao SPI, CS
// (automatico via spics_io_num) fica baixo do primeiro ao ultimo bit - Ao
// contrario do bit-bang antigo (CS low/high manual por chamada), aqui um
// comando multi-byte inteiro (ex: WREG = cmd+count+valor) e uma transacao
// so, exatamente como o CS ficava baixo por todo o grupo antes.
static void ads1248_transfer(const uint8_t *tx, uint8_t *rx, size_t len) {
	spi_transaction_t t = { };
	t.length = len * 8;
	t.tx_buffer = tx;
	t.rx_buffer = rx;
	ESP_ERROR_CHECK(spi_device_polling_transmit(ads1248_spi, &t));
}

static void ads1248_wreg(uint8_t reg_addr, uint8_t value) {
	uint8_t tx[3] = { (uint8_t) (ADS1248_CMD_WREG | (reg_addr & 0x0F)), 0x00,
			value };
	ads1248_transfer(tx, NULL, sizeof(tx));
}

static uint8_t ads1248_rreg(uint8_t reg_addr) {
	uint8_t tx[3] = { (uint8_t) (ADS1248_CMD_RREG | (reg_addr & 0x0F)), 0x00,
	ADS1248_CMD_NOP };
	uint8_t rx[3] = { };
	ads1248_transfer(tx, rx, sizeof(tx));
	return rx[2];
}

static void ads1248_command(uint8_t cmd) {
	uint8_t tx[1] = { cmd };
	ads1248_transfer(tx, NULL, sizeof(tx));
}

// Manda o comando RDATA e le os 24 bits do resultado da conversao, com
// sign-extend de complemento-de-2 pra um long de 32 bits. Comando e dados
// vao em duas transacoes separadas (CS mantido baixo entre elas via
// SPI_TRANS_CS_KEEP_ACTIVE) com um pequeno delay no meio, respeitando o
// tempo minimo entre o fim do comando e o inicio do dado (datasheet t6).
static long ads1248_rdata(void) {
	uint8_t cmd_tx[1] = { ADS1248_CMD_RDATA };
	spi_transaction_t cmd_t = { };
	cmd_t.length = 8;
	cmd_t.tx_buffer = cmd_tx;
	cmd_t.flags = SPI_TRANS_CS_KEEP_ACTIVE;
	ESP_ERROR_CHECK(spi_device_polling_transmit(ads1248_spi, &cmd_t));

	ets_delay_us(ADS1248_RDATA_T6_DELAY_US);

	uint8_t data_tx[3] = { ADS1248_CMD_NOP, ADS1248_CMD_NOP,
	ADS1248_CMD_NOP };
	uint8_t data_rx[3] = { };
	spi_transaction_t data_t = { };
	data_t.length = sizeof(data_tx) * 8;
	data_t.tx_buffer = data_tx;
	data_t.rx_buffer = data_rx;
	ESP_ERROR_CHECK(spi_device_polling_transmit(ads1248_spi, &data_t));

	uint32_t raw24 = ((uint32_t) data_rx[0] << 16)
			| ((uint32_t) data_rx[1] << 8) | data_rx[2];

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
	out_conf.pin_bit_mask = (1ULL << ADS_RESET_PIN) | (1ULL << ADS_START_PIN);
	gpio_config(&out_conf);

	gpio_config_t in_conf = { };
	in_conf.pull_down_en = (gpio_pulldown_t) 0;
	in_conf.pull_up_en = (gpio_pullup_t) 0;
	in_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	in_conf.mode = GPIO_MODE_INPUT;
	in_conf.pin_bit_mask = (1ULL << ADS_DRDY_PIN);
	gpio_config(&in_conf);

	gpio_set_level(ADS_RESET_PIN, HIGH);
	gpio_set_level(ADS_START_PIN, LOW);

	// SCLK/CS/DIN/DOUT saem do controle do periferico de hardware SPI, nao
	// sao mais GPIO manual.
	spi_bus_config_t buscfg = { };
	buscfg.mosi_io_num = ADS_DIN_PIN;
	buscfg.miso_io_num = ADS_DOUT_PIN;
	buscfg.sclk_io_num = ADS_SCLK_PIN;
	buscfg.quadwp_io_num = -1;
	buscfg.quadhd_io_num = -1;
	buscfg.max_transfer_sz = 8;

	ESP_ERROR_CHECK(spi_bus_initialize(ADS_SPI_HOST, &buscfg,
	SPI_DMA_DISABLED));

	spi_device_interface_config_t devcfg = { };
	devcfg.clock_speed_hz = ADS1248_SPI_CLOCK_HZ;
	devcfg.mode = 1; // CPOL=0, CPHA=1: DIN lido na descida, DOUT atualiza na subida
	devcfg.spics_io_num = ADS_CS_PIN;
	devcfg.queue_size = 1;
	// Margem extra de CS baixo antes/depois do primeiro/ultimo bit (em
	// meios-ciclos de SCLK), equivalente ao delay que o bit-bang antigo
	// dava com ADS1248_BITBANG_DELAY antes de comecar a chavear o clock.
	devcfg.cs_ena_pretrans = 2;
	devcfg.cs_ena_posttrans = 2;

	ESP_ERROR_CHECK(
			spi_bus_add_device(ADS_SPI_HOST, &devcfg, &ads1248_spi));
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
