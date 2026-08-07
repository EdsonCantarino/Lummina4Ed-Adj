# Comparacao ao vivo — ADS1248 vs CS5534 (cavidade 1, ampola vazia)

**Data:** 07/08/2026
**Branch:** `feature/config-web`
**Commit base do driver ADS1248:** `191f7e1` (ja commitado em sessao anterior, ver [[project_ads1248_driver_status]])
**Status:** nada novo commitado nesta sessao (so build/flash de teste + comparacao). `sdkconfig` da arvore de trabalho ficou de volta em `CONFIG_ADC_CHIP_CS5534=y` no fim da sessao — ver secao "Estado do repo" abaixo.

## Contexto

Sessao de bring-up da placa nova ADS1248 (dando sequencia ao trabalho de
07/08 registrado em `historico/`/memoria do checkpoint de bring-up). Duas
unidades Lummina4 fisicas disponiveis ao mesmo tempo:
- Unidade em **COM20**: placa nova, AD **ADS1248** (TI), populada nesta
  sessao de bring-up.
- Unidade em **COM21**: placa atual de producao, AD **CS5534** (Cirrus),
  usada como referencia conhecida pra comparar.

Objetivo: rodar o mesmo teste fisico (ampola sem liquido, controle
negativo esperado) nas duas ao mesmo tempo e comparar o valor bruto do
conversor AD e o resultado final da classificacao.

## O que foi feito

1. `sdkconfig`: `CONFIG_ADC_CHIP_CS5534` -> `CONFIG_ADC_CHIP_ADS1248`,
   build completo (`.\compila_Lummina4EdAdj.ps1`, fullclean+reconfigure+
   build, Docker `espressif/idf:release-v5.1`), NVS apagada
   (`erase_region 0x9000 0x6000`) e firmware gravado via esptool em
   **COM20**.
2. Confirmado no log serial (`[ADCDBG] chip=ADS1248 ch=0 raw=...`) que o
   driver novo esta lendo o canal de verdade — valores variando
   fisicamente (4580 -> 4576 -> 4349 numa checagem rapida antes do teste
   formal).
3. `sdkconfig` revertido pra `CONFIG_ADC_CHIP_CS5534=y`, build de novo, e
   firmware gravado em **COM21** (sem apagar NVS dessa vez — unidade de
   comparacao, sem motivo pra resetar config existente).
4. Ampola sem liquido inserida na cavidade 1 das duas unidades ao mesmo
   tempo (depois do CS5534 estabilizar em temperatura). Monitor serial
   (`miniterm`, background) rodando nas duas portas simultaneamente
   durante os 300s (5min) do ciclo completo.
5. Log de cada porta filtrado pelas linhas `[ADCDBG] ... ch=0 raw=...`
   (cavidade 1) e pelo resultado final da classificacao.

## Resultado

| | ADS1248 (COM20) | CS5534 (COM21) |
|---|---|---|
| Leituras no ciclo (300s) | 75 | 75 |
| Raw minimo | 3944 | 3859 |
| Raw maximo | 4454 | 4633 |
| Raw medio | 4159,1 | 4143,3 |
| Positive Percentage | 40,0% | 40,0% |
| Resultado final (Is Positive) | 0 (negativo) | 0 (negativo) |

Media difere ~0,4% entre os dois chips, faixas min/max sobrepostas, e a
classificacao final (`sensor / 100` do `ampoule_test.cpp`, **sem nenhum
reajuste de escala pro ADS1248**) bateu identica nas duas unidades:
negativo, 40% positivo. Ou seja, pra esse cenario (ampola vazia, sinal
fraco) a escala bruta dos dois chips ja fica proxima o bastante pra nao
exigir fator de conversao agressivo.

**Nao testado ainda:** o mesmo comparativo com ampola COM liquido/
indicador biologico real (sinal mais forte) — a diferenca de escala
poderia aparecer mais nesse regime. Fica pendente pra proxima sessao.

## Estado do repo ao fim da sessao

`sdkconfig` da arvore de trabalho ficou em `CONFIG_ADC_CHIP_CS5534=y` +
`CONFIG_ADC_DEBUG_SERIAL=y` (mesmo diff que ja estava pendente de commit
antes desta sessao comecar — so o debug serial ligado, nada novo). A
unidade em **COM20** continua fisicamente com o binario ADS1248 gravado
(nao reflete o `sdkconfig` atual da arvore) — se for reflashar essa
unidade, lembrar de trocar o Kconfig pra ADS1248 de novo antes do build.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`).
- Flash: `py -m esptool`, COM20 (ADS1248, NVS apagada antes) e COM21
  (CS5534, NVS preservada).
- Monitor serial: `py -m serial.tools.miniterm COMxx 115200 --raw` em
  background nas duas portas ao mesmo tempo, saida redirecionada pra
  arquivo de log e filtrada depois por `grep`.
