# 27/08 (tarde) — Aquecedor: bug do "desligado 6,43s" + unificação de tarefas + investigação de ruído no ADC

**Data:** 27/08/2026
**Branch:** `feature/config-web`
**Status:** código gravado e rodando na bancada (COM20). Parte validada
fisicamente, parte (limiar de validação do ADC) ainda em levantamento de
dado, sem decisão final.

Continuação da sessão do dia (ver
[[project_normal_eto_mode_status]] pra contexto de sessões anteriores);
essa parte não tem relação com o trabalho de i18n coreano/espanhol feito
mais cedo no mesmo dia (ver
`historico/2026-08-27-i18n-ticket-espanhol-coreano-tela-coreano.md`).

## 1. Bug encontrado: aquecedor ficava até 6,43s desligado sem motivo

Usuário filmou o LED do aquecedor durante um ciclo real e comparou com o
log serial do mesmo teste. Discrepância: o LED ficava apagado por até
6,43s seguidos, mesmo com a temperatura sempre abaixo do setpoint —
muito mais que os ~1,2s esperados (janela de silêncio antes da leitura
do ADC).

Causa raiz: `set_heater_controlling()` em `main/heater.cpp` não religava
o aquecedor imediatamente quando ele devia continuar ligado — dependia
passivamente da próxima amostra de temperatura chegar pra decidir, e essa
amostra vinha devagar (ver item 2). Corrigido com religamento antecipado:
se `heater_temperature > 0.0f && heater_temperature < setpoint -
HEATER_HYSTERESIS_C`, chama `heater_start()` na hora, em vez de esperar o
próximo ciclo de leitura.

## 2. Por que a amostra de temperatura vinha devagar

Investigado a pedido do usuário ("mas nao da para controlar a
temperatura com intervalos tao longos"). O DS18B20 estava configurado
pra resolução de 12 bits (750ms de conversão) e só um subconjunto dos
ciclos de leitura de fato lia o sensor de novo (havia um `RESCAN_INTERVAL
= 8` que reescaneava o barramento 1-Wire a cada 8 leituras, custando caro
toda vez).

Mudanças em `main/temperature.cpp` + `components/ds18x20/`:
- DS18B20 reconfigurado pra 9 bits (`TEMP_9_BIT = 0x1F` no scratchpad,
  93,75ms de conversão) — usuário confirmou que 1 leitura de temperatura
  por segundo em 9 bits é suficiente pro controle ("uma temperatura
  correta de 1 em 1 segundo em 9 bits para mim esta otimo").
- `LOOP_DELAY_MS`: 5000 → 900.
- Rescan do barramento 1-Wire deixou de ser periódico (a cada 8) e passou
  a ser **reativo**: só re-escaneia quando `sensor_count == 0` (ou seja,
  quando uma leitura falha). Antes disso, `ds18x20.c` tinha o delay de
  conversão hardcoded em 750ms (`SLEEP_MS(750)`); trocado por
  `DS18X20_CONVERSION_9BIT_MS = 100`.
- Confirmado por log que o byte de config do scratchpad realmente ficou
  em `0x1F` após a escrita (`ESP_LOGW` de verificação, lido de volta do
  sensor).

## 3. Ideia do usuário: unificar as duas tarefas (leitura de temp + controle do aquecedor)

Durante a investigação, o usuário perguntou por que existiam duas
tarefas de RTOS separadas se a decisão de ligar/desligar o aquecedor
depende inteiramente da leitura do DS18B20 — sugeriu juntar em uma
tarefa só. Expliquei a disputa de prioridades entre tarefas do FreeRTOS
(contexto secundário, não era a causa raiz), mas a causa raiz real era
mais simples: a tarefa consumidora antiga (`check_heater_temperature_task`)
tinha um `vTaskDelay(3000)` fixo dentro do laço, que somado ao tempo de
espera do `message buffer` produzia a cadência lenta observada — nenhuma
prioridade de tarefa explicava isso sozinha.

A unificação proposta pelo usuário resolveu isso de forma mais limpa que
só remover o delay:
- `main/heater.cpp`: `check_heater_temperature_task()` virou
  `void process_heater_temperature(float temperature)` — mesmo corpo,
  sem o `while(true)`/`xMessageBufferReceive`/`vTaskDelay(3000)` em volta.
  Removidas `check_heater_temperature_start_task()` /
  `check_heater_temperature_stop_task()` e a `TaskHandle_t` associada.
- `main/temperature.cpp`: em vez de `send_temperature_buffer(&temp_factor)`
  (grava num FreeRTOS message buffer pra outra tarefa consumir depois),
  chama `process_heater_temperature(temp_factor)` direto, na mesma
  tarefa que já leu o DS18B20. `send_temperature_buffer()` removida.
- `main/task_manager.cpp` / `main/include/task_manager.h`: removido
  `temperature_message_buffer`, `TEMP_BUFFER_SIZE`,
  `#include <freertos/message_buffer.h>` (não usado por mais nada).

## 4. Crash de boot introduzido pela unificação (encontrado e corrigido na mesma sessão)

Depois do merge, o firmware entrava em `Guru Meditation Error: Core 0
panic'ed (LoadProhibited)` no boot. Causa: `process_heater_temperature()`
chama `xTaskNotify(blink_led_heater_task_handle, ...)`, mas agora ela é
chamada diretamente de `temperature_setup()`, que roda **antes** de
`led_panel_setup()` (que é quem cria essa task handle) na sequência de
boot em `main.cpp`. A tarefa antiga era criada preguiçosamente (só no
primeiro uso), o que escondia essa dependência de ordem de boot por
acidente — a unificação expôs a dependência real.

Corrigido com uma flag de guarda: `static volatile bool
heater_processing_ready = false;` em `heater.cpp`, setada `true` logo no
início de `heater_start()` (após `gpio_set_level(HEATER_GPIO, ON)`), com
checagem no início de `process_heater_temperature()` (depois de já
atualizar `heater_temperature`, antes de qualquer notificação de LED) —
se ainda não está pronta, retorna sem tentar notificar a tarefa de LED.

## 5. Validação física do merge (bancada, COM20)

Capturado log serial após o fix do crash: sem panic, cadência de leitura
de temperatura confirmada em ~1020ms (bate com `LOOP_DELAY_MS = 900` +
overhead de conversão/E/S). Religamento antecipado do aquecedor
confirmado visualmente pelo usuário observando o LED físico durante
vários ciclos, incluindo um teste com ventilador sobre o bloco pra forçar
mais eventos de liga/desliga ("parece que ele compensa melhor a
temperatura").

Offset entre o DS18B20 e um termopar de referência (multímetro no furo de
calibração) ficou em ~3,5-4°C ao longo do teste — usuário confirmou que
esse offset não é uma preocupação nesta investigação ("o offset nao me
preocupa"), fica registrado só como contexto pra não confundir sessões
futuras.

## 6. Efeito colateral: aquecedor muito mais "elétrico" perto do ADC

Como o aquecedor agora liga/desliga com muito mais frequência (efeito
esperado e desejado do fix acima), o usuário levantou a hipótese de mais
ruído elétrico no ADS1248 (leitura de turbidez das ampolas) durante as
leituras. Comparação inicial usando delta percentual entre leituras
sucessivas (rodadas de teste, ~1-12s de espaçamento) sugeriu 6-10x mais
ruído — **métrica enganosa**, descartada: comparava sinais de ampolas
diferentes com magnitude de base muito diferente, então "% de variação"
não é comparável entre elas. Refeito com delta em contagem bruta do ADC:
aumento real, mas mais modesto (baseline antigo: max|delta| 1 e 3 contagens
em 2 cavidades; agora: max|delta| 2, 2, 3, 3 contagens em 4 cavidades).

### 6a. `HEATER_OFF_BEFORE_READ_MS` 100 → 300

`main/ampoule_test.cpp`: aumentada a janela de silêncio do aquecedor
antes de cada leitura do ADC de 100ms pra 300ms, tentando dar mais folga
elétrica. Buildado nesta sessão junto com o item 6b, gravado junto (ver
item 7).

### 6b. Instrumentação de 3 leituras consecutivas no ADS1248 (sem validação/rejeição ainda)

Usuário pediu pra não implementar direto um limiar de validação
("mas eu queria conversar antes... eu queria fazer algumas leituras, umas
3 com um delta maximo de x entre elas") sem antes ver dado real de quanto
3 leituras seguidas variam entre si no equipamento real — concordamos em
instrumentar primeiro, decidir o limiar depois.

`main/light_sensor_ads1248.cpp`, `read_channel_value()`: agora tira 3
conversões seguidas do mesmo canal (chip já está em conversão contínua,
~50ms entre elas, sem reescrever `MUX0` entre as 3 — reescrever reiniciaria
o filtro digital e custaria uma nova espera de DRDY completa). Só loga
`[s0,s1,s2] delta=max-min` — **nenhuma rejeição/fallback foi adicionada
ainda**, o valor retornado continua sendo a 1ª amostra (`s0`), mesmo
comportamento de antes. Fallback de timeout de DRDY inalterado (ainda usa
`last_valid_channel_value`/`channel_consecutive_timeouts`, mesmo padrão
de sempre).

## 7. Build + flash desta rodada (itens 6a + 6b juntos)

`compila_Lummina4EdAdj.ps1` rodou sem erro. Gravado via
`flash_Lummina4EdAdj.ps1 -Port COM20` (hash verificado, reset via RTS),
Wi-Fi reconectado (`netsh wlan connect name="MAX-C3B828"`). NVS não foi
apagada (não é o cenário do
[[feedback_erase_nvs_before_test_flash]]).

## 8. Captura de 5 minutos com teste de ampola rodando (bancada real)

Log salvo em `read_com20.py COM20 300` durante um teste completo nas 4
cavidades. Achados:

- **Sem crash, sem timeout de DRDY** em toda a captura.
- **Resultado biológico das 4 ampolas: negativado** em todas (variação
  percentual entre -5,7% e -12,8%, dentro do range esperado pra ampola
  estéril) — não relacionado ao trabalho de firmware desta sessão, só
  confirma que o teste completo roda de ponta a ponta sem regressão.
- **Achado principal sobre o ruído do ADC — muda o diagnóstico:**
  o delta entre as 3 leituras **não é ruído aleatório em todos os
  canais**. CH01 e CH04 mostram queda **sistemática e repetida** dentro
  da mesma janela de ~100-150ms (quase sempre `s0 > s1 > s2`, delta médio
  ~100-250 contagens no CH01, ~300-480 no CH04, com um pico isolado de
  1270 no CH04). CH02 e CH03 têm delta bem menor (7 a 60 contagens) e
  **sem direção fixa** (sobe e desce) — esse sim tem cara de ruído
  eletrônico de verdade.
- Hipótese levantada (não confirmada): a queda monotônica em CH01/CH04
  não parece ser assentamento do filtro digital do chip (esse já é
  respeitado pela espera de DRDY antes da 1ª leitura, e as leituras
  seguintes nem reescrevem o `MUX0`) — mais provável ser algo analógico
  *antes* do ADC (RC de entrada/anti-aliasing ainda convergindo pro novo
  canal) que continua "escorregando" além da 1ª conversão. Se for isso, a
  validação de 3-leituras-com-delta-máximo como pensada originalmente
  **não resolveria** o problema em CH01/CH04 — as 3 leituras vão sempre
  discordar de forma previsível (sinal em trânsito), não convergir.

## Pendente pra próxima sessão

- **Nenhum limiar de validação foi implementado ainda** — só a
  instrumentação de log (item 6b). Decisão de X leituras / delta máximo
  segue em aberto, e o achado do item 8 sugere que o desenho original
  (3 leituras, delta simétrico) pode não servir pra CH01/CH04 do jeito
  que está.
- Investigar CH01/CH04 com mais amostras seguidas (5-6) pra ver se a
  queda estabiliza em algum ponto ou é constante — ajudaria a decidir se
  o fix é aumentar o tempo de espera após troca de `MUX0`, usar a última
  amostra em vez da primeira, ou outra coisa.
- Entender por que CH02/CH03 não mostram o mesmo efeito que CH01/CH04
  (diferença física/elétrica entre os canais).
- Avaliar se `HEATER_OFF_BEFORE_READ_MS = 300` (item 6a, já gravado e
  rodando) teve efeito real na redução de ruído — a captura desta sessão
  não isolou esse fator separadamente do resto.
- `main/light_sensor_ads1248.cpp` ganhou log extra por leitura (3 valores
  + delta) — se decidir manter em produção, avaliar se o log fica verboso
  demais pra uso normal (fora de investigação).
