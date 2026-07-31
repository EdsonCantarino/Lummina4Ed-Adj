#include "ampoule_history.h"
#include <cstring>
#include <vector>
#include "esp_rom_crc.h"
#include "esp_log.h"
#include "nvs_helpers.h"

static const char *TAG = "AMP_HISTORY";

static const char *NVS_NAMESPACE = "amp_history";
static const char *NVS_KEY = "records_v1";

static const uint16_t MAGIC = 0xA55A;
static const uint8_t FORMAT_VERSION = 1;

// Tamanho de cada registro serializado, em bytes. Layout explícito e fixo
// (nao usa memcpy de struct C direto - evita depender de padding do
// compilador, sempre serializa campo a campo nessa ordem):
// id_test(4) + cavidade(1) + ciclo_minutos(2) + ts_inicio(4) + ts_fim(4) +
// resultado(1) + temperatura(1) = 17 bytes
static const size_t RECORD_SIZE = 17;

// magic(2) + version(1) + count(1) + write_index(1) + crc(4) = 9 bytes
static const size_t HEADER_SIZE = 9;

static ampoule_history_record_t cache[AMPOULE_HISTORY_MAX_RECORDS];
static uint8_t cache_count = 0;
static uint8_t cache_write_index = 0;

static void write_u32(std::vector<uint8_t> &buf, uint32_t v) {
	buf.push_back((uint8_t) (v & 0xFF));
	buf.push_back((uint8_t) ((v >> 8) & 0xFF));
	buf.push_back((uint8_t) ((v >> 16) & 0xFF));
	buf.push_back((uint8_t) ((v >> 24) & 0xFF));
}

static void write_u16(std::vector<uint8_t> &buf, uint16_t v) {
	buf.push_back((uint8_t) (v & 0xFF));
	buf.push_back((uint8_t) ((v >> 8) & 0xFF));
}

static uint32_t read_u32(const uint8_t *p) {
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16)
			| ((uint32_t) p[3] << 24);
}

static uint16_t read_u16(const uint8_t *p) {
	return (uint16_t) p[0] | ((uint16_t) p[1] << 8);
}

static void serialize_record(std::vector<uint8_t> &buf,
		const ampoule_history_record_t &r) {
	write_u32(buf, r.id_test);
	buf.push_back(r.cavidade);
	write_u16(buf, r.ciclo_minutos);
	write_u32(buf, r.ts_inicio);
	write_u32(buf, r.ts_fim);
	buf.push_back((uint8_t) r.resultado);
	buf.push_back(r.temperatura);
}

static void deserialize_record(const uint8_t *p, ampoule_history_record_t &r) {
	r.id_test = read_u32(p);
	r.cavidade = p[4];
	r.ciclo_minutos = read_u16(p + 5);
	r.ts_inicio = read_u32(p + 7);
	r.ts_fim = read_u32(p + 11);
	r.resultado = (ampoule_history_result_t) p[15];
	r.temperatura = p[16];
}

static uint32_t compute_crc(const std::vector<uint8_t> &buf,
		size_t crc_field_offset) {
	// CRC cobre tudo exceto os proprios 4 bytes do campo de CRC.
	uint32_t crc = esp_rom_crc32_le(0, buf.data(), crc_field_offset);
	size_t after = crc_field_offset + 4;
	if (buf.size() > after) {
		crc = esp_rom_crc32_le(crc, buf.data() + after, buf.size() - after);
	}
	return crc;
}

static void persist() {
	std::vector<uint8_t> buf;
	buf.reserve(HEADER_SIZE + AMPOULE_HISTORY_MAX_RECORDS * RECORD_SIZE);

	write_u16(buf, MAGIC);
	buf.push_back(FORMAT_VERSION);
	buf.push_back(cache_count);
	buf.push_back(cache_write_index);
	size_t crc_offset = buf.size();
	write_u32(buf, 0); // placeholder do CRC

	for (int i = 0; i < AMPOULE_HISTORY_MAX_RECORDS; i++) {
		serialize_record(buf, cache[i]);
	}

	uint32_t crc = compute_crc(buf, crc_offset);
	buf[crc_offset] = (uint8_t) (crc & 0xFF);
	buf[crc_offset + 1] = (uint8_t) ((crc >> 8) & 0xFF);
	buf[crc_offset + 2] = (uint8_t) ((crc >> 16) & 0xFF);
	buf[crc_offset + 3] = (uint8_t) ((crc >> 24) & 0xFF);

	NVSHelper nvs;
	nvs.begin(NVS_NAMESPACE);

	if (!nvs.setBlob(NVS_KEY, buf, true)) {
		ESP_LOGE(TAG, "Falha ao gravar historico na NVS");
	}
}

void ampoule_history_load() {
	memset(cache, 0, sizeof(cache));
	cache_count = 0;
	cache_write_index = 0;

	NVSHelper nvs;
	nvs.begin(NVS_NAMESPACE);

	std::vector<uint8_t> buf = nvs.getBlob(NVS_KEY);

	size_t expected_size = HEADER_SIZE
			+ AMPOULE_HISTORY_MAX_RECORDS * RECORD_SIZE;

	if (buf.size() != expected_size) {
		ESP_LOGI(TAG,
				"Nenhum historico valido na NVS (primeiro boot ou formato antigo) - iniciando vazio");
		return;
	}

	uint16_t magic = read_u16(buf.data());
	uint8_t version = buf[2];

	if (magic != MAGIC || version != FORMAT_VERSION) {
		ESP_LOGW(TAG, "Historico com magic/versao inesperados - iniciando vazio");
		return;
	}

	uint32_t stored_crc = read_u32(buf.data() + 5);
	uint32_t calc_crc = compute_crc(buf, 5);

	if (stored_crc != calc_crc) {
		ESP_LOGW(TAG, "CRC do historico nao confere - iniciando vazio");
		return;
	}

	cache_count = buf[3];
	cache_write_index = buf[4];

	if (cache_count > AMPOULE_HISTORY_MAX_RECORDS)
		cache_count = AMPOULE_HISTORY_MAX_RECORDS;

	if (cache_write_index >= AMPOULE_HISTORY_MAX_RECORDS)
		cache_write_index = 0;

	for (int i = 0; i < AMPOULE_HISTORY_MAX_RECORDS; i++) {
		deserialize_record(buf.data() + HEADER_SIZE + i * RECORD_SIZE,
				cache[i]);
	}

	ESP_LOGI(TAG, "Historico carregado da NVS: %d registro(s)", cache_count);
}

esp_err_t ampoule_history_add(const ampoule_history_record_t &record) {
	cache[cache_write_index] = record;

	cache_write_index = (cache_write_index + 1) % AMPOULE_HISTORY_MAX_RECORDS;

	if (cache_count < AMPOULE_HISTORY_MAX_RECORDS)
		cache_count++;

	persist();

	return ESP_OK;
}

int ampoule_history_get_count() {
	return cache_count;
}

void ampoule_history_clear() {
	memset(cache, 0, sizeof(cache));
	cache_count = 0;
	cache_write_index = 0;

	persist();

	ESP_LOGI(TAG, "Historico apagado");
}

bool ampoule_history_get_record(int index_from_newest,
		ampoule_history_record_t &out) {
	if (index_from_newest < 0 || index_from_newest >= cache_count)
		return false;

	// cache_write_index aponta para a PROXIMA posicao livre (mais antiga a
	// ser sobrescrita). O mais recente gravado esta em write_index-1.
	int newest_slot = (cache_write_index - 1 + AMPOULE_HISTORY_MAX_RECORDS)
			% AMPOULE_HISTORY_MAX_RECORDS;

	int slot = (newest_slot - index_from_newest + AMPOULE_HISTORY_MAX_RECORDS)
			% AMPOULE_HISTORY_MAX_RECORDS;

	out = cache[slot];

	return true;
}
