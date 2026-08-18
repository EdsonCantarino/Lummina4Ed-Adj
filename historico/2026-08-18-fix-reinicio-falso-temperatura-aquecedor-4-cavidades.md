# Checkpoint 18/08 — reinício falso por temperatura durante teste + aquecedor sem folga com 4 cavidades

**Data:** 18/08/2026
**Branch:** `feature/config-web`
**Commit:** `dfe68c4` (pushed)
**Status:** compilado (dois `.bin`, CS5534 e ADS1248), **nada gravado no equipamento ainda, nada enviado ao cliente**.

## Relato do cliente (WhatsApp)

Com um teste em andamento, a resistência parece parar de funcionar até o
teste finalizar ou a ampola ser removida. A luz de identificação da
resistência fica piscando durante o teste (fixa depois de finalizar).
Em testes longos (1h) a temperatura cai até sair da faixa programada,
soa o alarme e o equipamento reinicia sozinho. Reportado tanto na placa
antiga (CS5534) quanto na nova (ADS1248) — firmwares enviados no dia
anterior.

## Diagnóstico (log de campo fornecido pelo cliente, COM4, placa CS5534)

Cliente mandou `monitor_COM4_2026-08-18_152344.log.txt` (salvo em
Downloads). Achados, em ordem de descoberta:

1. **Causa direta do "reinício com alarme":** `RESTART_ID=5` — a regra
   adicionada em 14/08 (`main/ampoules.cpp`, checkpoint
   `2026-08-14-trava-config-durante-teste-e-reset-ampola-aquecimento.md`)
   pra resetar quando uma ampola está presente e a temperatura ainda não
   chegou na liberação (`heater_release_temp_c`) — pensada pro cenário de
   ligar o equipamento com ampola já inserida antes do aquecedor esquentar.
   O log mostra ela disparando **no meio de um teste já rodando há 156s**
   de um ciclo de 3600s (`"Esta em teste: Sim"` no log, temp=54,5 <
   liberação=55,0). A condição original não distinguia esse caso do de
   boot.

2. **Causa de fundo (por que a temperatura caía de verdade):** com as 4
   cavidades ativas, ler as 4 ampolas em sequência mediu **~4,5s reais**
   no log (`Tempo total de execucao: 4507` — unidade do printf está
   errada, é ms não us), contra um `loop_cycle_time_s` configurado de
   **4s** (preset Normal/ETO, `advanced_config.html`). Como o cálculo do
   delay entre ciclos (`ampoules_test_timer_task`) nunca pode ficar
   negativo, ele caía direto no piso de segurança de 100ms — ou seja, os
   ciclos rodavam quase nas costas um do outro, sem folga real pro
   aquecedor. Somado a isso, `prepare_test()`/`finalize_test()` desligam o
   aquecedor durante toda a janela de captura do LED (`led_capture_time_s`
   = 0,5s) + o tempo da leitura do ADC (~0,6s) **por cavidade** — com 4
   cavidades, o aquecedor ficava desligado quase o ciclo inteiro,
   religando só por janelas de poucos milissegundos entre uma cavidade e
   outra. Confirmado que a temperatura caiu de 57,5°C pra 54,5°C em ~3
   minutos nesse regime.

A validação de segurança já existente (`advanced_config_min_loop_cycle_time`)
assume só +0,5s de overhead por cavidade — real medido foi ~0,63s — então
o preset passava raspando na validação (`4 >= 4`) mas na prática já
estourava.

## Fix (discutido e decidido em conversa com o usuário antes de implementar)

Três mudanças, todas em código compartilhado entre as duas placas (não
mexe em nada específico de ADC):

1. **`ampoules.cpp`** — `RESTART_ID=5` ganhou a condição `!is_any_testing()`:
   só reseta se nenhum teste já estiver rodando. Resolve o sintoma do
   reinício indevido.

2. **`ampoule_test.cpp`, `prepare_test()`** — reestruturado a pedido do
   usuário: o aquecedor só desliga nos **últimos 100ms** antes da leitura
   do ADC (não mais durante a janela de captura inteira). Religa
   (`set_heater_controlling(true)`) assim que a leitura termina, antes
   até de desligar o LED UV. Isso derruba o tempo de aquecedor-desligado
   por cavidade de ~1,1-1,2s pra ~100ms + tempo do ADC.

3. **`ampoule_test.cpp`, `ampoules_test_timer_task()`** — piso de folga
   entre rodadas de leitura subiu de 100ms pra **2s** (`HEATER_RECOVERY_MIN_MS`),
   garantido **independente** do `loop_cycle_time_s` configurado — corrige
   também qualquer unidade já em campo com valor antigo/insuficiente
   gravado na memória, sem precisar resalvar nada pela tela.

Como a causa raiz ficou resolvida na origem (itens 2 e 3), os **presets
Normal/ETO voltaram pra `loopCycleTime: 4`** (chegou a subir pra 6
temporariamente durante a investigação, revertido depois — ver conversa).

Além disso, mudança independente pedida no início da sessão:
**`printer.cpp`**: `PRINTER_RESTART_GRACE_MS` de 10s pra 7s (carência do
watchdog de recuperação de impressora — não relacionado ao bug acima,
avaliado como seguro porque o corte do beep de alarme já é protegido por
outro mecanismo, `ampoule_finalize_in_progress`).

## Build

Dois `.bin` gerados (`compila_Lummina4EdAdj.ps1`, Docker + ESP-IDF 5.1),
salvos em `build_releases/` (não versionado):
- `Lummina4_MAXXIMED_v2026_08_18_CS5534.bin`
- `Lummina4_MAXXIMED_v2026_08_18_ADS1248.bin`

`.gz` de `advanced_config.html` **não precisou regenerar** — o preset
teve ida e volta (4→6→4), o conteúdo final bateu exatamente com o já
commitado (só thumbprint do gzip mudava, revertido pra evitar diff
espúrio).

## Pendente

- Nada gravado no equipamento nem enviado ao cliente ainda.
- Validação física: nenhuma ainda. Precisa confirmar em bancada que a
  temperatura se mantém estável com as 4 cavidades ativas por um teste
  longo (idealmente reproduzir o cenário de 1h do cliente) antes de
  mandar pro campo.
- `sdkconfig` do repo permanece em ADS1248 + `CONFIG_ADC_DEBUG_SERIAL=y`
  (estado de HEAD, não alterado nesta sessão apesar de ter sido alternado
  temporariamente pra gerar os dois builds).
