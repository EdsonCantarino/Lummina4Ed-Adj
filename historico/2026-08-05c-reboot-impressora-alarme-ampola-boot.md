# Reboot da impressora sem retirar ampola + alarme de ampola presente no boot

**Data:** 05/08/2026
**Branch:** `feature/config-web`
**Status:** Item 1 (impressora) implementado e **confirmado fisicamente**. Item 2 (alarme de boot) implementado mas **ainda não confirmado** — teste ao vivo mostrou que a trava não está segurando.

## Contexto

Cliente reportou dois problemas via WhatsApp:
1. "Quando impressora travada esta fazendo reboot no final do ciclo sem retirar ampola"
2. "Ligando com a ampola ela na esta entrando na condicao de alarme"

## Causa raiz (achada em conjunto, discutida antes de implementar)

Os dois problemas vêm da mesma raiz: o firmware distingue mal "teste em andamento" (`is_testing()`, vira `false` assim que o ciclo termina) de "ampola fisicamente presente" (`ampoule_any()`/sensor, só vira `false` quando é removida de verdade).

## Item 1 — Watchdog de impressora (`main/printer.cpp`) — CONFIRMADO

Desenho decidido com o usuário: trocar a trava de `is_testing()` por `!ampoule_any()`, e trocar o disparo baseado em timers (15s impressão travada / 30s desconectada) pelo flag `is_printer_error()` do driver USB (`usb_class_driver.cpp`, reflete falha real de transferência).

```cpp
static void attempt_safe_printer_recovery(const char *reason) {
    if (ampoule_any()) {
        // adia
        return;
    }
    esp_restart();
}
```
No loop do watchdog: `if (is_printer_error()) { attempt_safe_printer_recovery(...); }`.

Removidos: `PRINT_HANG_TIMEOUT_MS`, `PRINTER_DISCONNECTED_TIMEOUT_MS`, `print_operation_started_ms`, `printer_disconnected_since_ms`, `printer_ever_connected`, `any_cavity_testing()` — decisão explícita do usuário ("não vejo necessidade do timer visto que não vai fazer diferença no final").

**Teste físico:** ciclo de 5min disparado na cavidade 1 com o cabo USB da impressora desconectado. Ao terminar o ciclo, log mostrou repetidamente:
```
W PRINTER: Erro de impressora detectado (is_printer_error), mas ha ampola em alguma cavidade - adiando reinicio ate ficar seguro
```
Confirma que o reinício fica corretamente adiado enquanto a ampola (já testada) continua na cavidade — exatamente o comportamento pedido.

## Item 2 — Alarme de ampola presente no boot (`main/ampoule_sensor.cpp`)

### Tentativa 1 (NÃO funcionou) — semear a leitura inicial

Causa identificada: `read_ampoules()` começava o array de debounce (`confirmed_present[4]`) hardcoded como `{false,false,false,false}`, então a primeira iteração do loop sempre "confirmava" ausência (batendo com esse valor inicial) antes do debounce ter qualquer chance de ler o sensor de verdade — derrubando `is_ampoules_present_in_init` permanentemente logo no início.

**Fix tentado:** semear `confirmed_present[]` com uma leitura real (`read_pin_majority`) antes do loop, em vez de hardcode `false`.

**Teste físico ao vivo (equipamento ligado com ampola já inserida na cavidade 1):**
- Log mostrou "Funcoes Habilitadas: Não" por só ~3 segundos (7006ms→10026ms) e depois "Sim" — a trava não segurou.
- Às ~21.5s, o firmware iniciou um teste normal na ampola 1 (contador 1851, "Executando teste: 1") como se ela tivesse acabado de ser inserida — exatamente o sintoma que o fix deveria ter evitado.
- **Essa tentativa não resolveu o problema.** Causa exata da liberação prematura não foi totalmente isolada.

### Tentativa 2 (implementada, aguardando teste físico) — contador de 10s de ausência contínua

Desenho do usuário: em vez de depender de uma leitura confirmada acertar de primeira, usar um contador de tempo que **zera toda vez que qualquer ampola é detectada** e só libera a trava (`is_ampoules_present_in_init = false`) depois de **10 segundos contínuos** sem nenhuma detecção. Robusto contra os problemas de timing/ordem de inicialização da tentativa 1, porque não depende da primeira leitura estar certa - mesmo que o debounce demore ~1-2s pra "pegar" a presença real, isso já reseta o contador de sobra antes dos 10s completarem.

```cpp
#define AMPOULE_ABSENT_CONFIRM_MS (10 * 1000)
// ...
if (is_ampoules_present_in_init) {
    if (is_ampoules) {
        no_ampoule_since_ms = 0;
    } else if (10s contínuos sem ampola) {
        is_ampoules_present_in_init = false;
    }
}
```

**Gravado no equipamento, ainda não testado fisicamente com ampola presente no boot** (a sessão seguiu pra outros itens). Testar: ligar com ampola inserida, confirmar que "Funções Habilitadas" fica "Não" e o alarme soa continuamente enquanto ela não for removida; remover e confirmar que libera ~10s depois.

## Commit

`main/printer.cpp` e `main/ampoule_sensor.cpp` — ambos gravados no equipamento de testes (COM20) e commitados/pushados nesta sessão, mesmo com o item 2 pendente de confirmação (documentado aqui pra não se perder).
