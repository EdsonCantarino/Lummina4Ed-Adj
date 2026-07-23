#ifndef ADVANCED_CONFIG_H_
#define ADVANCED_CONFIG_H_

#include <stdint.h>
#include "esp_err.h"

typedef struct {
	float led_capture_time_s;     // 0.5 a 7.0
	uint32_t loop_cycle_time_s;   // 2 a 50
	uint8_t samples_initial;      // 3 a 10
	uint8_t samples_final;        // 3 a 10
	bool cavity_enabled[4];
	uint32_t early_check_time_s;  // 180 a 900 (3 a 15 minutos)
} advanced_config_t;

extern const advanced_config_t ADVANCED_CONFIG_DEFAULTS;
extern advanced_config_t g_advanced_config;
extern bool g_advanced_config_crc_error;

// Carrega a configuracao das duas areas da NVS (dupla area + CRC).
// Deve ser chamada uma unica vez no boot, antes de qualquer leitura de
// g_advanced_config.
void advanced_config_load();

// Grava a configuracao nas duas areas (grava area1, le e verifica, grava
// area2, le e verifica). So atualiza g_advanced_config e limpa
// g_advanced_config_crc_error se as duas gravacoes forem confirmadas.
esp_err_t advanced_config_save(const advanced_config_t &cfg);

// Restaura os valores padrao de fabrica (grava nas duas areas).
esp_err_t advanced_config_restore_defaults();

int advanced_config_cavities_enabled_count();

// Minimo de tempo de ciclo (item 2), em segundos, dado o tempo de captura
// configurado e o numero de cavidades habilitadas.
uint32_t advanced_config_min_loop_cycle_time(float led_capture_time_s,
		int cavities_enabled_count);

// Minimo de tempo de checagem antecipada (item 5), em segundos, dado o
// numero de amostras (iniciais + finais) e o tempo de ciclo.
uint32_t advanced_config_min_early_check_time(uint8_t samples_initial,
		uint8_t samples_final, uint32_t loop_cycle_time_s);

#endif
