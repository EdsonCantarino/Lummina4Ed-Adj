# 28/08 — Fix de troca antecipada de canal (ADS1248) + tabela pra teste de jumper

**Data:** 28/08/2026
**Branch:** `feature/config-web`
**Status:** código implementado, compilado e gravado (COM20). Validado com
ampola real (captura de 3min abaixo) — melhora clara no delta de CH01/CH04.
Teste de jumper nos 4 canais ainda não realizado (tabela no fim, vazia).

Continuação da investigação de ruído no ADC iniciada em
[[2026-08-27b-aquecedor-tarefa-unica-ruido-adc]] e
[[2026-08-27b-dados-3-leituras-adc]]. Nessa sessão anterior, o
levantamento de 3 leituras consecutivas por canal mostrou dois perfis
distintos:

- **CH01/CH04**: delta grande e quase sempre monotônico (s0 ≥ s1 ≥ s2)
  — hipótese: sinal ainda "em trânsito" (assentamento analógico/RC do
  fotodiodo) após a troca de canal, não ruído aleatório.
- **CH02/CH03**: delta bem menor, sem direção fixa em quase metade das
  rodadas — perfil mais condizente com ruído eletrônico de verdade.

## Experimento planejado

Usuário vai tirar o sensor de cada uma das 4 cavidades e colocar um
jumper (baixíssima impedância) no lugar, um canal de cada vez,
repetindo a mesma captura de log que já existe no firmware
(`read_channel_value()` em `main/light_sensor_ads1248.cpp`, log
`Read CHxx = raw [s0,s1,s2] delta=D`). Nenhuma mudança de código é
necessária pra essa captura — a instrumentação de 3 amostras já está
gravada e rodando.

**O que cada resultado indicaria:**
- Se o delta **sumir/zerar** com o jumper num canal: o ruído daquele
  canal vem do sensor/fotodiodo (capacitância, corrente de escuro,
  RC de entrada) — não da placa/ADC.
- Se o delta **persistir** parecido com o jumper: aponta pra algo comum
  ao canal/placa (acoplamento do aquecedor chaveando, ruído de
  referência, etc.), não do sensor.
- Fazer isso nos **4 canais** cobre as duas hipóteses de uma vez: CH01/
  CH04 testam a hipótese de assentamento RC do fotodiodo; CH02/CH03
  testam a hipótese de ruído eletrônico genérico da placa.
- Sugestão (ainda não confirmada com o usuário) de repetir cada canal
  com o aquecedor ligado e desligado, pra separar "ruído de canal" de
  "ruído induzido pelo chaveamento do aquecedor" no mesmo teste.

## Fix implementado: troca antecipada de canal (100ms + 200ms)

Ideia discutida com o usuário: em vez de deixar a troca de canal (MUX0)
acontecer só no fim da janela de aquecedor-desligado (dentro de
`read_channel_value()`, como era antes), trocar no **meio** dela — 100ms
depois que o aquecedor desliga, troca o canal, mais 200ms de assentamento
antes de disparar a leitura real. Orçamento total de
`HEATER_OFF_BEFORE_READ_MS` (300ms) não muda, só redistribui.

Implementação (`main/ampoule_test.cpp`, `main/light_sensor_ads1248.cpp`,
`main/light_sensor.cpp`, `main/include/light_sensor.h`):
- `read_channel_value()` (ADS1248 e CS5534, mesma API pública dos dois
  drivers) foi dividida em `light_sensor_select_channel()` (só escreve
  MUX0/seleciona o canal) + `light_sensor_read_selected_channel()` (espera
  DRDY e lê, sem trocar canal de novo) — `read_channel_value()` continua
  existindo como wrapper das duas, pra quem não precisa do controle fino
  (ex.: `ampoule_test_check_cavity()`, rotina de calibração).
- `prepare_test(int ampoule, int channel = -1)` ganhou o parâmetro
  `channel` opcional. Com `channel >= 0`: espera `CHANNEL_SWITCH_DELAY_MS`
  (100ms), troca o canal, espera o resto da janela (200ms). Sem parâmetro
  (`-1`, comportamento antigo): espera os 300ms inteiros sem trocar canal,
  preservando o fluxo antigo pra quem chama sem precisar da troca
  antecipada.
- `ampoule_test()` (loop principal de teste, `main/ampoule_test.cpp`) foi
  atualizado pra `prepare_test(ampoule, index)` +
  `light_sensor_read_selected_channel(index)` em vez de
  `prepare_test(ampoule)` + `read_channel_value(index)`.

Build oficial (`compila_Lummina4EdAdj.ps1`, fullclean+reconfigure+build,
`espressif/idf:release-v5.1`) rodou sem erros/warnings nos 3 arquivos
alterados. Gravado via `flash_Lummina4EdAdj.ps1 -Port COM20` — hash
verificado, reset via RTS.

## Validação com ampola real (captura de 3min, firmware novo)

Captura serial de 180s via script Python (pyserial, 115200 baud),
extraindo linhas `Read CHxx = raw [s0,s1,s2] delta=D` e `Temperatura: X`.
61 leituras no total. Comparação direta com a baseline de
[[2026-08-27b-dados-3-leituras-adc]] (firmware antigo, mesma
metodologia):

| Canal | delta médio ANTES (27/08) | delta médio AGORA (28/08) | mono ANTES (s0≥s1≥s2) | mono AGORA |
|-------|---------------------------:|----------------------------:|------------------------:|-------------:|
| CH01  | 123,9 | 107,3 (37,4 sem o outlier da rodada 8) | 22/23 | 13/15 |
| CH02  | 35,7  | 67,5 (26,4 sem o outlier da rodada 15) | 13/24 | 7/15 |
| CH03  | 35,7  | 20,1 | 12/24 | 5/16 |
| CH04  | **401,5** | **45,9** | **24/24** | **7/15** |

**CH04** (pior caso antes) teve o delta médio reduzido em ~8,7x e deixou
de ser monotônico quase todo o tempo (24/24 → 7/15) — forte indício de
que a troca antecipada de canal resolveu o "sinal em trânsito" nesse
canal. **CH01** também melhorou bastante (123,9 → ~37 tirando o outlier),
mas continua mais monotônico que CH02/CH03 (13/15) — pode ainda ter um
resíduo do mesmo efeito, ou já estar mais perto do comportamento de
ruído real dos outros dois canais. **CH03** melhorou também (35,7 →
20,1). **CH02** teve um outlier isolado grande (rodada 15, delta=642)
que puxa a média pra cima, mas sem ele fica em linha com o valor
anterior.

Duas leituras isoladas com pico grande — CH01 rodada 8 (delta=1079) e
CH02 rodada 15 (delta=642) — não têm o padrão sistemático de antes
(não se repetem nas rodadas vizinhas). **Ampolas usadas nesta captura
estavam vazias** (sem reação biológica em andamento) — o que descarta a
hipótese de variação real de turbidez explicando esses picos (50ms entre
amostras já seria curto demais pra isso mesmo com ampola real, mas com
ampola vazia fica ainda mais claro: não há processo físico que explique
o salto). Sobra evento elétrico/mecânico pontual como explicação mais
provável — não investigado a fundo ainda, precisa de mais dados pra
identificar a causa.

Como consequência do mesmo ponto: por não ter reação biológica mudando
o sinal, esta captura é uma medida limpa do piso de ruído real do ADC —
a melhora medida (CH04 ~8,7x, CH01 ~3,3x tirando o outlier) não é
mascarada por sinal variando, reforça que é ruído/assentamento sendo
reduzido de fato.

### Dados brutos (rodadas completas com as 4 cavidades, ~12s entre rodadas)

Rodada 0 e 15 ficaram parciais porque a captura começou/terminou no meio
de uma rodada (nem todos os 4 canais foram lidos dentro da janela de
180s).

| Rodada | Tempo | CH1 [s0,s1,s2] Δ | CH2 [s0,s1,s2] Δ | CH3 [s0,s1,s2] Δ | CH4 [s0,s1,s2] Δ | Temp °C |
|---|---|---|---|---|---|---|
| 0 (parcial) | ~09:16:36 | - | - | [-27,-36,-34] Δ9 | [65534,65528,65514] Δ20 | - |
| 1 | ~09:16:47 | [67704,67682,67648] Δ56 | [68667,68660,68611] Δ56 | [6,-6,-19] Δ25 | [65971,65898,65796] Δ175 | 59,5 |
| 2 | ~09:16:59 | [67288,67295,67238] Δ57 | [68290,68292,68278] Δ14 | [-75,-67,-69] Δ8 | [64976,64995,65005] Δ29 | 59,5 |
| 3 | ~09:17:11 | [67112,67086,67077] Δ35 | [68125,68121,68081] Δ44 | [-56,-72,-69] Δ16 | [64546,64528,64508] Δ38 | 59,5 |
| 4 | ~09:17:23 | [67081,67052,67041] Δ40 | [68252,68220,68224] Δ32 | [-44,-61,-51] Δ17 | [65520,65557,65585] Δ65 | 59,5 |
| 5 | ~09:17:35 | [67093,67090,67078] Δ15 | [68024,68037,68011] Δ26 | [-30,-27,-30] Δ3 | [64432,64420,64410] Δ22 | 59,0 |
| 6 | ~09:17:47 | [67085,67079,67058] Δ27 | [68010,68011,68008] Δ3 | [-43,-21,-33] Δ22 | [64486,64448,64454] Δ38 | 59,0 |
| 7 | ~09:17:59 | [67185,67165,67159] Δ26 | [68072,68078,68072] Δ6 | [1,-16,-23] Δ24 | [64569,64555,64562] Δ14 | 59,0 |
| 8 | ~09:18:11 | [65807,66268,66886] **Δ1079** | [68284,68285,68311] Δ27 | [-89,-72,-80] Δ17 | [64921,64875,64857] Δ64 | 58,5 |
| 9 | ~09:18:23 | [67118,67101,67085] Δ33 | [68096,68080,68078] Δ18 | [-38,-60,-68] Δ30 | [64635,64626,64641] Δ15 | 58,5 |
| 10 | ~09:18:35 | [67160,67137,67104] Δ56 | [68168,68129,68133] Δ39 | [-53,-40,-42] Δ13 | [64678,64658,64636] Δ42 | 58,5 |
| 11 | ~09:18:47 | [67244,67229,67201] Δ43 | [68110,68107,68103] Δ7 | [-49,-72,-93] Δ44 | [64784,64788,64812] Δ28 | 58,5 |
| 12 | ~09:18:59 | [67245,67221,67187] Δ58 | [67990,67990,67965] Δ25 | [-27,-28,-44] Δ17 | [64622,64601,64595] Δ27 | 58,5 |
| 13 | ~09:19:11 | [67134,67127,67115] Δ19 | [68099,68083,68064] Δ35 | [-89,-74,-51] Δ38 | [63542,63618,63614] Δ76 | 58,5 |
| 14 | ~09:19:23 | [66825,66800,66788] Δ37 | [67442,67452,67413] Δ39 | [1,5,0] Δ5 | [64131,64096,64107] Δ35 | 59,0 |
| 15 (parcial) | ~09:19:35 | [66601,66593,66572] Δ29 | [66771,66462,66129] **Δ642** | [-91,-76,-57] Δ34 | - | 59,0 |

Log bruto completo salvo em
`serial_capture_280826.log` (scratchpad da sessão, não versionado).

### Mesmos dados, aplicando o arredondamento real do firmware (`/ 100`)

`main/ampoule_test.cpp` linha 808 faz `sensor = sensor / 100;` antes de
usar a leitura pra qualquer coisa (comparação de turbidez, histórico,
etc.) — é divisão inteira (trunca, não arredonda pro mais próximo).
Aplicando esse mesmo truncamento nas 3 amostras de cada rodada acima, o
delta que sobra é o que **realmente importa** pra decisão do teste (o
delta "cru" registrado no log é mais fino que a resolução que o
algoritmo de fato usa):

| Rodada | CH1 (s0,s1,s2)/100 Δ | CH2 (s0,s1,s2)/100 Δ | CH3 (s0,s1,s2)/100 Δ | CH4 (s0,s1,s2)/100 Δ |
|---|---|---|---|---|
| 0 (parcial) | - | - | 0,0,0 Δ0 | 655,655,655 Δ0 |
| 1 | 677,676,676 Δ1 | 686,686,686 Δ0 | 0,0,0 Δ0 | 659,658,657 Δ2 |
| 2 | 672,672,672 Δ0 | 682,682,682 Δ0 | 0,0,0 Δ0 | 649,649,650 Δ1 |
| 3 | 671,670,670 Δ1 | 681,681,680 Δ1 | 0,0,0 Δ0 | 645,645,645 Δ0 |
| 4 | 670,670,670 Δ0 | 682,682,682 Δ0 | 0,0,0 Δ0 | 655,655,655 Δ0 |
| 5 | 670,670,670 Δ0 | 680,680,680 Δ0 | 0,0,0 Δ0 | 644,644,644 Δ0 |
| 6 | 670,670,670 Δ0 | 680,680,680 Δ0 | 0,0,0 Δ0 | 644,644,644 Δ0 |
| 7 | 671,671,671 Δ0 | 680,680,680 Δ0 | 0,0,0 Δ0 | 645,645,645 Δ0 |
| 8 | 658,662,668 **Δ10** | 682,682,683 Δ1 | 0,0,0 Δ0 | 649,648,648 Δ1 |
| 9 | 671,671,670 Δ1 | 680,680,680 Δ0 | 0,0,0 Δ0 | 646,646,646 Δ0 |
| 10 | 671,671,671 Δ0 | 681,681,681 Δ0 | 0,0,0 Δ0 | 646,646,646 Δ0 |
| 11 | 672,672,672 Δ0 | 681,681,681 Δ0 | 0,0,0 Δ0 | 647,647,648 Δ1 |
| 12 | 672,672,671 Δ1 | 679,679,679 Δ0 | 0,0,0 Δ0 | 646,646,645 Δ1 |
| 13 | 671,671,671 Δ0 | 680,680,680 Δ0 | 0,0,0 Δ0 | 635,636,636 Δ1 |
| 14 | 668,668,667 Δ1 | 674,674,674 Δ0 | 0,0,0 Δ0 | 641,640,641 Δ1 |
| 15 (parcial) | 666,665,665 Δ1 | 667,664,661 **Δ6** | 0,0,0 Δ0 | - |

Nessa resolução, o delta some quase por completo: **0 ou 1** em praticamente
todas as rodadas dos 3 canais com sinal (CH1/CH2/CH4) — ler `s0` (1
amostra) ou fazer qualquer combinação das 3 dá o mesmo resultado final
usado pelo teste. CH03 fica sempre `0,0,0` porque o sinal cru dele nesta
captura já era perto de zero (não é zero por causa do arredondamento).

Os 2 outliers sobrevivem ao truncamento (CH1 rodada 8: ainda Δ10 em 671,
~1,5%; CH2 rodada 15: ainda Δ6 em ~665, ~0,9%) — bem menores que no dado
cru, mas ainda visíveis. Reforça a mesma conclusão de antes: nas rodadas
normais, 1 leitura basta; as 3 leituras só teriam valor pra pegar esses
eventos raros.

## Tabela pra preencher durante o teste do jumper (ainda pendente)

Cada canal tem 3 valores (`s0, s1, s2`, mesmo formato do log serial).

Leitura 1  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 2  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 3  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 4  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 5  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 6  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 7  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 8  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 9  | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 10 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 11 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 12 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 13 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 14 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 15 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 16 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 17 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 18 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 19 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 20 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 21 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 22 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 23 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 24 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 25 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 26 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 27 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 28 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 29 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____
Leitura 30 | Tempo: ____ | Canal1: ___, ___, ___ | Canal2: ___, ___, ___ | Canal3: ___, ___, ___ | Canal4: ___, ___, ___ | Temperatura: ____ C | Obs: ____

## Pendente pra próxima sessão

- Realizar o teste físico do jumper (4 canais, com e sem aquecedor
  ligado) e preencher a tabela acima — agora serve pra isolar se o
  resíduo de ruído que sobrou (principalmente em CH01) é sensor ou
  placa/ADC, já com o fix de troca antecipada de canal já em produção.
- Investigar os 2 outliers isolados (CH01 rodada 8, CH02 rodada 15) —
  não teve tempo de amostra suficiente nesta sessão pra saber se é
  pontual/elétrico ou sinal biológico real da ampola.
- Validar se `CHANNEL_SWITCH_DELAY_MS = 100` é o valor certo ou se vale
  testar outra divisão (ex.: 150/150, ou 50/250) — só um valor foi
  testado nesta sessão.
- Confirmar teste completo de ampola de ponta a ponta com o firmware
  novo (essa captura foi só de observação do ADC, não confirmou
  positivo/negativo em campo).
