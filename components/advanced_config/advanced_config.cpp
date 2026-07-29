#include "advanced_config.h"
#include <cstring>
#include <vector>
#include "esp_rom_crc.h"
#include "esp_log.h"
#include "nvs_helpers.h"

static const char *TAG = "ADV_CONFIG";

const advanced_config_t ADVANCED_CONFIG_DEFAULTS = {
	2.0f,                        // led_capture_time_s (comportamento atual)
	30,                          // loop_cycle_time_s (comportamento atual)
	5,                           // samples_initial (comportamento atual)
	5,                           // samples_final (comportamento atual)
	{ true, true, true, true },  // cavity_enabled (comportamento atual)
	420,                         // early_check_time_s = 7 minutos (comportamento atual)
	37.0f,                       // heater_setpoint_c (comportamento atual)
	33.0f,                       // heater_min_temp_c (comportamento atual)
	43.0f,                       // heater_max_temp_c (comportamento atual)
	35.0f,                       // heater_release_temp_c (comportamento atual)
	OPERATION_MODE_NORMAL        // operation_mode (padrao de fabrica = Normal)
};

advanced_config_t g_advanced_config = ADVANCED_CONFIG_DEFAULTS;
bool g_advanced_config_crc_error = false;

typedef struct {
	advanced_config_t data;
	uint32_t crc;
} advanced_config_blob_t;

static const char *NVS_NAMESPACE = "adv_config";
static const char *KEY_AREA_A = "adv_cfg_a";
static const char *KEY_AREA_B = "adv_cfg_b";

static uint32_t compute_crc(const advanced_config_t &cfg) {
	return esp_rom_crc32_le(0, (const uint8_t*) &cfg,
			sizeof(advanced_config_t));
}

enum class AreaState {
	ABSENT, CORRUPT, OK
};

static const char* area_state_name(AreaState state) {
	switch (state) {
	case AreaState::ABSENT:
		return "AUSENTE";
	case AreaState::CORRUPT:
		return "CORROMPIDA";
	default:
		return "OK";
	}
}

static void log_config(const char *prefix, const advanced_config_t &cfg) {
	ESP_LOGI(TAG,
			"%s: captura=%.1fs looping=%lus amostras=%d+%d checagem=%lus cavidades=[%d,%d,%d,%d] temp[setpoint=%.1f min=%.1f liberacao=%.1f max=%.1f] modo=%s",
			prefix, cfg.led_capture_time_s,
			(unsigned long) cfg.loop_cycle_time_s, cfg.samples_initial,
			cfg.samples_final, (unsigned long) cfg.early_check_time_s,
			cfg.cavity_enabled[0], cfg.cavity_enabled[1],
			cfg.cavity_enabled[2], cfg.cavity_enabled[3],
			cfg.heater_setpoint_c, cfg.heater_min_temp_c,
			cfg.heater_release_temp_c, cfg.heater_max_temp_c,
			cfg.operation_mode == OPERATION_MODE_CRC1 ? "CRC1" :
			cfg.operation_mode == OPERATION_MODE_ETO ? "ETO" : "Normal");
}

static AreaState read_area(const char *key, advanced_config_t &out) {
	NVSHelper nvs;
	nvs.begin(NVS_NAMESPACE);

	std::vector<uint8_t> blob = nvs.getBlob(key);

	if (blob.empty())
		return AreaState::ABSENT;

	if (blob.size() != sizeof(advanced_config_blob_t)) {
		ESP_LOGW(TAG,
				"%s: tamanho do blob inesperado (%d bytes, esperado %d)", key,
				(int) blob.size(), (int) sizeof(advanced_config_blob_t));
		return AreaState::CORRUPT;
	}

	advanced_config_blob_t wrapper;
	memcpy(&wrapper, blob.data(), sizeof(wrapper));

	if (compute_crc(wrapper.data) != wrapper.crc) {
		ESP_LOGW(TAG, "%s: CRC nao confere (esperado 0x%08lx, calculado 0x%08lx)",
				key, (unsigned long) wrapper.crc,
				(unsigned long) compute_crc(wrapper.data));
		return AreaState::CORRUPT;
	}

	out = wrapper.data;
	return AreaState::OK;
}

static bool write_area(const char *key, const advanced_config_t &cfg) {
	advanced_config_blob_t wrapper;
	wrapper.data = cfg;
	wrapper.crc = compute_crc(cfg);

	NVSHelper nvs;
	nvs.begin(NVS_NAMESPACE);

	std::vector<uint8_t> blob((uint8_t*) &wrapper,
			(uint8_t*) &wrapper + sizeof(wrapper));

	if (!nvs.setBlob(key, blob, true))
		return false;

	// Le de volta e verifica antes de considerar a gravacao valida.
	advanced_config_t verify;
	if (read_area(key, verify) != AreaState::OK)
		return false;

	return memcmp(&verify, &cfg, sizeof(advanced_config_t)) == 0;
}

esp_err_t advanced_config_save(const advanced_config_t &cfg) {
	log_config("Gravando configuracao avancada", cfg);

	if (!write_area(KEY_AREA_A, cfg)) {
		ESP_LOGE(TAG, "Falha ao gravar/verificar a Area A - gravacao abortada");
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Area A gravada e verificada com sucesso");

	if (!write_area(KEY_AREA_B, cfg)) {
		ESP_LOGE(TAG, "Falha ao gravar/verificar a Area B - gravacao abortada");
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Area B gravada e verificada com sucesso");

	g_advanced_config = cfg;
	g_advanced_config_crc_error = false;

	return ESP_OK;
}

esp_err_t advanced_config_restore_defaults() {
	ESP_LOGW(TAG, "Restaurando configuracao avancada para o padrao de fabrica");
	return advanced_config_save(ADVANCED_CONFIG_DEFAULTS);
}

void advanced_config_load() {
	advanced_config_t a { };
	advanced_config_t b { };

	AreaState state_a = read_area(KEY_AREA_A, a);
	AreaState state_b = read_area(KEY_AREA_B, b);

	ESP_LOGI(TAG, "Boot: Area A = %s | Area B = %s", area_state_name(state_a),
			area_state_name(state_b));

	if (state_a == AreaState::ABSENT && state_b == AreaState::ABSENT) {
		// Primeiro boot: equipamento nunca foi configurado, nao e erro.
		ESP_LOGI(TAG,
				"Nenhuma configuracao encontrada (primeiro boot) - gravando padrao de fabrica");
		g_advanced_config = ADVANCED_CONFIG_DEFAULTS;
		g_advanced_config_crc_error = false;
		advanced_config_save(ADVANCED_CONFIG_DEFAULTS);
		return;
	}

	if (state_a == AreaState::OK && state_b == AreaState::OK
			&& memcmp(&a, &b, sizeof(advanced_config_t)) == 0) {
		ESP_LOGI(TAG, "Area A e Area B validas e identicas - configuracao ok");
		g_advanced_config = a;
		g_advanced_config_crc_error = false;
		log_config("Configuracao efetiva carregada", g_advanced_config);
		return;
	}

	// CRC invalido em qualquer area, ausencia assimetrica (uma area existe
	// e a outra nao), ou valores divergentes entre areas com CRC valido:
	// nenhuma area "vence" sozinha - opera com o padrao de fabrica em
	// memoria ate o operador confirmar a restauracao pela web.
	ESP_LOGE(TAG,
			"Inconsistencia na configuracao avancada (CRC invalido ou Area A != Area B) - operando com padrao de fabrica ate restauracao manual");
	g_advanced_config = ADVANCED_CONFIG_DEFAULTS;
	g_advanced_config_crc_error = true;
	log_config("Configuracao efetiva (padrao de fabrica, erro pendente)",
			g_advanced_config);
}

int advanced_config_cavities_enabled_count() {
	int count = 0;

	for (int i = 0; i < 4; i++) {
		if (g_advanced_config.cavity_enabled[i])
			count++;
	}

	return count;
}

uint32_t advanced_config_min_loop_cycle_time(float led_capture_time_s,
		int cavities_enabled_count) {
	if (cavities_enabled_count < 1)
		cavities_enabled_count = 1;

	float min_time = cavities_enabled_count * (led_capture_time_s + 0.5f);

	return (uint32_t) (min_time + 0.999f); // arredonda para cima
}

uint32_t advanced_config_min_early_check_time(uint8_t samples_initial,
		uint8_t samples_final, uint32_t loop_cycle_time_s) {
	return (uint32_t) (samples_initial + samples_final) * loop_cycle_time_s;
}
