#ifndef AMPOULE_HISTORY_H_
#define AMPOULE_HISTORY_H_

#include <stdint.h>
#include "esp_err.h"

#define AMPOULE_HISTORY_MAX_RECORDS 64

typedef enum {
	AMPOULE_RESULT_NEGATIVE = 0,
	AMPOULE_RESULT_POSITIVE = 1,
	AMPOULE_RESULT_CANCELLED = 2,
} ampoule_history_result_t;

typedef struct {
	uint32_t id_test;
	uint8_t cavidade;         // 1 a 4
	uint16_t ciclo_minutos;
	uint32_t ts_inicio;       // timestamp Unix (UTC do RTC do equipamento)
	uint32_t ts_fim;
	ampoule_history_result_t resultado;
	uint8_t temperatura;
	bool printed;             // true = ja saiu impresso com sucesso alguma vez
} ampoule_history_record_t;

// Carrega o histórico da NVS para a cache em RAM. Chamar uma única vez no
// boot. Se não houver dado gravado ou o CRC não conferir, começa vazio
// (não é dado crítico como a configuração avançada - não justifica dupla
// área nem bloquear o equipamento).
void ampoule_history_load();

// Adiciona um registro ao final do buffer circular (FIFO - descarta o mais
// antigo ao ultrapassar AMPOULE_HISTORY_MAX_RECORDS) e persiste na NVS.
esp_err_t ampoule_history_add(const ampoule_history_record_t &record);

// Quantidade de registros válidos hoje (0 a AMPOULE_HISTORY_MAX_RECORDS).
int ampoule_history_get_count();

// index_from_newest = 0 é o registro mais recente, 1 o penúltimo, etc.
// Retorna false se o índice estiver fora do intervalo [0, count).
bool ampoule_history_get_record(int index_from_newest,
		ampoule_history_record_t &out);

// Marca um registro (por id_test) como impresso com sucesso. Usado tanto
// pela impressao ao vivo (teste recem concluido) quanto pela reimpressao
// manual/automatica - no-op se o id_test nao for encontrado.
void ampoule_history_mark_printed(uint32_t id_test);

// Apaga todo o histórico (usado no reset de dados do usuário).
void ampoule_history_clear();

#endif
