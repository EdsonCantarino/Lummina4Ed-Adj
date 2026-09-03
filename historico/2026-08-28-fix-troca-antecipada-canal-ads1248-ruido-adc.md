# 28/08 — Fix de troca antecipada de canal (ADS1248) + leitura única + réplica no CS5534

**Data:** 28/08/2026
**Branch:** `feature/config-web`
**Status:** commitado e pushado (`e9fb523`), gravado e validado fisicamente
na bancada (COM20) com ampola real. CS5534 recebeu a mesma técnica por
simetria de código, **sem validação física** (sem hardware disponível).

Continuação direta da investigação de ruído no ADC de 27/08 (ver
[[2026-08-27b-aquecedor-tarefa-unica-ruido-adc]] e
[[2026-08-27b-dados-3-leituras-adc]]). Dados brutos, tabelas e a
comparação detalhada ficaram em
[[teste280826_Ruido_AD]] (`historico/Testes/`) — este arquivo é o resumo
da sessão.

## 1. Hipótese confirmada e fix aplicado

O levantamento de 27/08 (3 leituras seguidas por canal) mostrou CH01/CH04
caindo de forma quase sempre monotônica entre as amostras — sinal
compatível com assentamento analógico ainda em andamento após a troca de
canal do ADS1248, não ruído aleatório.

Fix: mover a troca de canal (escrita em `MUX0`) pro **meio** da janela de
aquecedor-desligado, em vez do fim (como acontecia antes, dentro de
`read_channel_value()`). Implementação:

- `main/include/light_sensor.h` / `main/light_sensor_ads1248.cpp`:
  `read_channel_value()` dividida em `light_sensor_select_channel()` (só
  troca o canal) + `light_sensor_read_selected_channel()` (espera DRDY e
  lê, sem trocar de novo). `read_channel_value()` continua existindo como
  wrapper das duas, usada por quem não precisa do controle fino (ex.:
  `ampoule_test_check_cavity()`, calibração de cavidade).
- `main/ampoule_test.cpp`: `prepare_test(int ampoule, int channel = -1)`
  ganhou o parâmetro `channel` opcional. Com `channel >= 0` (usado só pelo
  loop principal de teste, `ampoule_test()`): espera
  `CHANNEL_SWITCH_DELAY_MS` (100ms), troca o canal, espera
  `CHANNEL_SETTLE_MS` (ver item 3), aí quem chama lê com
  `light_sensor_read_selected_channel()`. Sem `channel` (comportamento
  antigo): espera os `HEATER_OFF_BEFORE_READ_MS` (300ms) inteiros sem
  trocar canal — preserva o fluxo da calibração de cavidade sem alteração.

### Validação com ampola real (3min, ampolas vazias)

Comparado direto com a baseline de 27/08 (mesma metodologia, 3 amostras
por leitura ainda ligada nesse momento):

| Canal | delta médio ANTES | delta médio DEPOIS | mono ANTES | mono DEPOIS |
|---|---:|---:|---:|---:|
| CH01 | 123,9 | 37,4 (sem outlier) | 22/23 | 13/15 |
| CH02 | 35,7 | 26,4 (sem outlier) | 13/24 | 7/15 |
| CH03 | 35,7 | 20,1 | 12/24 | 5/16 |
| CH04 | **401,5** | **45,9** | **24/24** | **7/15** |

CH04 (pior caso) teve o delta médio reduzido em ~8,7x. Ampolas vazias
nessa captura — sem reação biológica mudando o sinal, então a melhora é
medida limpa do piso de ruído, não mascarada por sinal variando. Detalhe
completo em [[teste280826_Ruido_AD]].

## 2. Leitura simplificada de volta pra 1 amostra

A instrumentação de 3 leituras (`s0,s1,s2` + delta) criada em 27/08 só
servia pra levantar dado, nunca rejeitava/validava nada. Depois do fix do
item 1, o delta entre 1 e 3 amostras deixou de fazer diferença prática:
na resolução real usada pelo teste (`sensor / 100` em
`ampoule_test.cpp`), o delta ficou 0 ou 1 em quase todas as rodadas — só
os 2 outliers isolados (1 de CH01, 1 de CH02) ainda apareciam.

Revertido pra 1 leitura só (`light_sensor_read_selected_channel()`
simplificada) — menos código, mesmo resultado prático, e o tempo real de
aquecedor-desligado não aumenta com isso (ver nota no código:
"desloca 2 conversões extras descartadas pra assentamento aproveitado de
fato", não adiciona tempo novo).

## 3. `CHANNEL_SETTLE_MS`: 200 → 300 → 400ms (decisão sem evidência forte)

Testado com ampola real rodando um ciclo completo de 300s (5min), 4
cavidades, comparando CV% (desvio padrão ÷ média, valores `/100`
arredondados, rodadas de transiente térmico removidas):

| Config | Teste 1 | Teste 2 |
|---|---|---|
| 300ms | CH01-04: 1,10%-1,31% | CH01-04: 3,72%-4,87% |
| 400ms | CH01-04: 2,19%-2,92% | CH01-04: 2,02%-2,45% |

**Não deu pra concluir nada com confiança** — a variação entre as 2
repetições do *mesmo* valor (300ms: 1,2% vs 4,3%) foi maior que a
diferença entre 300 e 400ms. Também não são diretamente comparáveis
porque a reação biológica de cada ampola teve magnitude bem diferente
entre os testes (CV mistura ruído com sinal real mudando). Ficou em
**400ms** por decisão do usuário ao final da sessão, não por evidência
estatística — precisaria de mais repetições de cada lado (idealmente com
ampola vazia, controlando a variável de sinal) pra decidir com confiança.

4 testes completos de ciclo (300s, 4 cavidades) rodados no total — 2x
300ms, 2x 400ms — todos sem crash/timeout de DRDY, todos com resultado
NEGATIVADO coerente e impressão de ticket funcionando.

## 4. Réplica da técnica no driver CS5534 (não testada)

A pedido do usuário, a mesma técnica foi replicada em `main/light_sensor.cpp`
(driver CS5534, compilado só quando `CONFIG_ADC_CHIP_CS5534` — hoje
desligado, build atual usa `CONFIG_ADC_CHIP_ADS1248`):

- `light_sensor_select_channel()`: removido o `vTaskDelay(50ms)` interno
  que existia depois de trocar canal/OffSet/Gain — o assentamento passa a
  ser controlado inteiramente por quem chama (`prepare_test()`), igual no
  ADS1248. A sequência "100ms → troca canal → 400ms → lê" já vem de
  `prepare_test()`, que é compartilhado entre os dois drivers (chama as
  funções pelo nome genérico) — só precisou tirar o delay interno que
  brigava com essa lógica.
- `light_sensor_read_selected_channel()`: simplificada de 5 conversões
  (usava a última) pra 1 conversão só.

**Sem hardware CS5534 disponível nesta sessão pra validar fisicamente.**
Como o chip está desligado no build atual, esse código fica morto/
inatingível no `.bin` gerado — não afeta o firmware que foi gravado e vai
pro cliente, mas fica pendente de validação se algum dia esse driver for
usado de novo.

## 5. Commit e push

Commit `e9fb523` — "fix: troca antecipada de canal no ADS1248 reduz
ruido do ADC" — cobre os 4 arquivos de código (`ampoule_test.cpp`,
`light_sensor.h`, `light_sensor.cpp`, `light_sensor_ads1248.cpp`) +
`historico/Testes/teste280826_Ruido_AD.md`. Pushado pro
`origin/feature/config-web` (`dce50db..e9fb523`). Ficaram de fora do
commit `.claude/settings.local.json` (config local) e os `.docx` em
`doc/` (não fazem parte do controle de versão do projeto, ver
[[project_gz_manual_regen]] pra outros exemplos do mesmo padrão).

O `.bin` gravado na bancada corresponde exatamente a esse commit —
rastreabilidade ok pra mandar pro cliente testar em mais unidades.

## Pendente pra próxima sessão

- Aguardar retorno dos testes do cliente em campo (mais unidades,
  situações diferentes das da bancada) — vai dar mais dado real que uma
  unidade só na bancada consegue.
- Decidir com mais confiança 300ms vs 400ms — precisaria de mais
  repetições, idealmente com ampola vazia pra isolar ruído de sinal real.
- Investigar os 2 outliers isolados (CH01, CH02) que sobreviveram ao fix
  — não foi possível determinar a causa nesta sessão.
- Validar fisicamente a réplica no driver CS5534 quando/se esse hardware
  estiver disponível de novo.
- Ainda não implementado: nenhuma rejeição/validação baseada em delta
  entre leituras (a ideia original de "3 leituras com delta máximo") —
  ficou descartada depois que o fix do item 1 reduziu o problema o
  suficiente pra não precisar mais disso.
