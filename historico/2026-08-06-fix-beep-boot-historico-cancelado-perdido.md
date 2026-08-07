# Fix — beep falso no boot sem ampola + histórico/reimpressão perdidos no cancelamento

**Data:** 06/08/2026
**Branch:** `feature/config-web`
**Commit:** `b547804` (pushed)
**Status:** os 2 problemas reportados pelo cliente foram corrigidos e validados fisicamente (múltiplas vezes). Reimpressão automática ao reconectar a impressora e um teste completo (não cancelado) até o fim **ainda não foram testados** nesta sessão.

## Contexto

Cliente reportou dois problemas novos (WhatsApp) na sequência do fix de ontem (`2ef9564`, "reinicio de impressora so com ampola removida + trava de boot com ampola presente"):

1. "Ao ligar a Lummina 4 ela beepa algumas vezes e não devia" — bug que nós criamos no fix de ontem.
2. Na versão anterior, cancelados e concluídos eram tratados igual; agora o firmware não gera histórico dos cancelados nem faz a reimpressão.

## 1. Beep falso no boot sem ampola nenhuma (`main/ampoule_sensor.cpp`, `main/ampoules.cpp`)

### Causa raiz

O fix de ontem introduziu `is_ampoules_present_in_init`, uma trava de boot que começa **sempre `true`** (mesmo sem ampola nenhuma instalada) e só vira `false` depois de confirmar 10s contínuos sem detecção (`AMPOULE_ABSENT_CONFIRM_MS`). Enquanto essa trava está ativa, `read_ampoules_test_task()` (`main/ampoules.cpp`) piscava os 4 LEDs de erro **e** dava 3 beeps de `buzzer_alarm()` a cada ~250ms — ou seja, **todo boot beepava e piscava por até 10s, com ou sem ampola presente**.

### Fix

Nova flag `is_ampoules_confirmed_present_in_init` (`ampoule_sensor.cpp`), atualizada a cada ciclo de leitura com o valor já calculado de presença confirmada (`is_ampoules`). Em `ampoules.cpp`, o alarme (LED + buzzer) só dispara quando essa flag é `true` — a trava de botões continua ativa durante toda a janela de incerteza (comportamento seguro, sem mudança), mas o alarme sonoro/visual só quando há ampola de fato confirmada.

**Validado fisicamente:** ligar sem ampola em nenhuma cavidade → sem beep, sem LED piscando. Ligar com ampola já inserida → alarme soa/pisca normalmente até a remoção.

## 2. Histórico e reimpressão perdidos ao cancelar teste com impressora em erro (`main/ampoule_test.cpp`, `main/printer.cpp`)

### Causa raiz (achada por reprodução física, não só leitura de código)

Reproduzimos ao vivo: ampola numa cavidade, impressora com cabo desconectado, teste rodando, ampola removida no meio do teste. Log serial mostrou o ESP32 **reiniciando sozinho no instante da remoção**, antes do teste cancelado aparecer no histórico web (`/history` ficou `0/0`).

Sequência exata (confirmada pelo log):
1. `alarm_status=3` (ampola removida durante teste) detectado.
2. Firmware tenta imprimir o ticket do cancelamento (`print_ampoule_test(i, true)`) — falha (`usb_host_transfer_submit Out fail`), seta `is_printer_error()`.
3. No mesmo segundo, o watchdog de impressora (`print_check_status`, fix de ontem) vê `ampoule_any()` já falso (ampola acabou de sair) e chama `esp_restart()` — **antes** de `set_history()` rodar.
4. Reboot. Registro do cancelamento nunca foi persistido — nada pra reimprimir depois, mesmo reconectando a impressora.

Ou seja: a própria remoção que dispara o cancelamento também zera `ampoule_any()`, criando uma corrida entre a task que grava o histórico e a task do watchdog que reinicia o equipamento.

### Fix

**a) Nova flag `ampoule_finalize_in_progress`** (`ampoule_test.cpp`/`.h`), setada `true` no início de qualquer finalização/cancelamento de teste e só liberada depois que a etapa relevante termina:
- `finalize_ampoule_test()` (conclusão normal) — libera logo após `set_history()`.
- `abort_ampoule_test_sensor_fault()` (falha persistente de sensor) — libera após `set_history()` + o beep de erro.
- Ramo "ampola removida durante teste" (`alarm_status==3`) — libera só **depois** do beep de alarme completo (4 beeps + tom longo), não antes. Na primeira versão do fix a flag liberava antes do beep, e o watchdog reiniciava no meio dele, cortando o som (percebido pelo cliente/usuário como "beep de erro muito curto").
- Cancelamento por temperatura fora do range — libera após o loop das 4 cavidades.

`attempt_safe_printer_recovery()` (`printer.cpp`) agora checa `ampoule_any() || is_ampoule_finalize_in_progress()` antes de considerar reiniciar.

**b) Carência de 30s antes do reinício de fato** (`PRINTER_RESTART_GRACE_MS`, `printer.cpp`): mesmo depois de "seguro" (sem ampola, sem finalização em andamento), o watchdog exige 30 segundos contínuos nesse estado antes de chamar `esp_restart()` — contador (`printer_safe_to_restart_since_ms`) zera a cada detecção de ampola ou finalização em andamento, mesmo padrão do `AMPOULE_ABSENT_CONFIRM_MS` já usado na trava de boot. Decisão de design discutida com o usuário nesta sessão (valor final 30s, não 10s — ver histórico da conversa se precisar do porquê da hesitação entre os dois números).

### Validado fisicamente (2x, testes independentes)

- Teste 1 (id 1855): ampola cavidade 1, impressora desconectada, removida em ~48s de um teste de 5min → cancelamento apareceu no histórico web (`Cancelado`, 60°C, 5 min) depois do reinício.
- Teste 2 (id 1856): mesmo cenário, log mostrou a contagem de carência subindo linearmente de 2000ms até 29000/30000ms antes do `esp_restart()` real, confirmando o mecanismo de carência funcionando como projetado.
- Confirmado via `GET /api/v1/history?maxResults=64` (bypassando cache do navegador, que mostrou dado desatualizado numa checagem) que os dois registros (`idTest 1855` e `1856`, `resultado: "C"`) persistiram corretamente.

### Pendente pra próxima sessão

- **Reimpressão automática ao reconectar a impressora** — os 2 tickets cancelados (1855, 1856) ficaram pendentes (impressora nunca foi reconectada nesta sessão, usuário optou por não testar agora). Falta conectar o cabo e confirmar que saem sozinhos.
- **Teste completo (positivo/negativo) até o fim, sem cancelar** — só testamos cancelamentos hoje. Vale confirmar que a flag `ampoule_finalize_in_progress` não interferiu no caminho normal de conclusão (`finalize_ampoule_test`).

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`) — precisou reabrir o Docker Desktop no início da sessão (estava fechado).
- Flash: `esptool`/`py -m esptool`, COM20, NVS apagada (`erase_region 0x9000 0x6000`) antes de cada flash de teste.
- Monitor serial: `python -m serial.tools.miniterm COM20 115200 --raw` em background — matar o processo real (`Get-CimInstance Win32_Process | Where CommandLine -like '*miniterm*'`, olhar o `Name: python.exe`, não os wrappers bash) antes de flashar.
- Wi-Fi do equipamento de teste: AP `MAX-4907A8`, senha `56275627` (senha por número de série, achada no log de boot — usuário estava tentando uma senha antiga `45874587` que não é mais válida pra esse equipamento).
- Tela web: `http://192.168.10.10/history` — **cuidado**: a tela pode mostrar dado em cache do navegador depois de um reboot do equipamento; conferir via `GET /api/v1/history` direto se o resultado parecer desatualizado.
