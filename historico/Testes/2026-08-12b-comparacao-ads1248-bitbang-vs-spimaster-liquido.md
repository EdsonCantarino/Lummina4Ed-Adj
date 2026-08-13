# Teste comparativo — ADS1248 bit-bang (COM20) vs ADS1248 spi_master (COM22), ampola COM líquido

**Data:** 12/08/2026
**Branch:** `feature/config-web`
**Objetivo:** repetir o comparativo bit-bang vs spi_master (ver `2026-08-12-comparacao-ads1248-bitbang-vs-spimaster.md`, feito com ampola vazia) usando ampola **com líquido**, a situação operacional real — e checar se o sinal mais forte satura a entrada do ADC (preocupação levantada antes de rodar este teste).

## Placas

Mesmo setup do teste anterior, sem nenhuma alteração de firmware:
- **COM20**: ADS1248, driver bit-bang.
- **COM22**: ADS1248, driver spi_master.

**Logs brutos:** `monitor_com20_bitbang_liquido_12_08.log`, `monitor_com22_spimaster_liquido_12_08.log`.

## Resultado

| | ADS1248 bit-bang (COM20) | ADS1248 spi_master (COM22) |
|---|---|---|
| Leituras no ciclo | 75 | 75 |
| Raw mínimo | 37460 | 38726 |
| Raw máximo | 43032 | 42377 |
| Raw médio | 38819,3 | 39931,2 |
| Positive Percentage | 40,0% | 40,0% |
| Variação percentual | -6,76% | -1,77% |
| Resultado final (Is Positive) | 0 (negativo) | 0 (negativo) |

## Saturação — não observada

Inspecionada a sequência completa das 75 leituras nas duas placas: variação suave e contínua ponto a ponto, sem nenhum valor repetido/grudado num teto (que seria a assinatura de clipping analógico ou overflow do registro do ADC). Raw ficou entre ~37k e ~43k, bem abaixo de qualquer limite conhecido do ADC nessa configuração (PGA=1). Não há indício de saturação neste teste.

## Comparação com o teste de ampola vazia (mesma sessão, mais cedo)

| | Vazia → Líquido | Variação |
|---|---|---|
| bit-bang (COM20) | 28105,5 → 38819,3 | +38,1% |
| spi_master (COM22) | 30817,0 → 39931,2 | +29,6% |

O raw sobe visivelmente com líquido nas duas placas, como esperado — confirma que o teste com ampola vazia estava mesmo na faixa baixa da operação real, não é um artefato de driver ou de sinal fraco demais pra ser representativo.

## bit-bang vs spi_master

Proporção spi_master/bit-bang: ~1,029 (médias 39931,2 vs 38819,3, ~2,9% de diferença) — bem menor que os ~10% observados no teste com ampola vazia mais cedo hoje. Classificação final idêntica (negativo, 40%) nas duas capturas, como em todos os testes anteriores desta investigação.

## Valores brutos (raw) — todas as 75 leituras, na ordem capturada

| # | bit-bang (COM20) | spi_master (COM22) |
|---|---|---|
| 1 | 43032 | 42377 |
| 2 | 42640 | 42226 |
| 3 | 42302 | 41978 |
| 4 | 41977 | 41700 |
| 5 | 41696 | 41523 |
| 6 | 41319 | 41317 |
| 7 | 40994 | 40608 |
| 8 | 40674 | 40478 |
| 9 | 40440 | 40317 |
| 10 | 40163 | 40160 |
| 11 | 39950 | 39958 |
| 12 | 39746 | 39859 |
| 13 | 39524 | 39738 |
| 14 | 39289 | 39573 |
| 15 | 39144 | 39453 |
| 16 | 38975 | 39426 |
| 17 | 38795 | 39305 |
| 18 | 38658 | 39217 |
| 19 | 38534 | 39136 |
| 20 | 38365 | 38956 |
| 21 | 38267 | 38929 |
| 22 | 38210 | 38944 |
| 23 | 38065 | 38856 |
| 24 | 38012 | 38859 |
| 25 | 37948 | 38855 |
| 26 | 37890 | 38876 |
| 27 | 37813 | 38816 |
| 28 | 37777 | 38820 |
| 29 | 37828 | 38726 |
| 30 | 37708 | 39053 |
| 31 | 37702 | 39139 |
| 32 | 37654 | 39078 |
| 33 | 37611 | 39059 |
| 34 | 37578 | 39025 |
| 35 | 37543 | 39038 |
| 36 | 37485 | 38999 |
| 37 | 37503 | 38997 |
| 38 | 37460 | 39012 |
| 39 | 37514 | 39031 |
| 40 | 37607 | 39033 |
| 41 | 37652 | 39009 |
| 42 | 37681 | 39045 |
| 43 | 37697 | 39186 |
| 44 | 37762 | 39248 |
| 45 | 37809 | 39314 |
| 46 | 37860 | 39430 |
| 47 | 37900 | 39434 |
| 48 | 37978 | 39594 |
| 49 | 38042 | 39617 |
| 50 | 38172 | 39702 |
| 51 | 38147 | 39785 |
| 52 | 38234 | 39868 |
| 53 | 38264 | 39883 |
| 54 | 38344 | 39926 |
| 55 | 38374 | 40090 |
| 56 | 38438 | 40135 |
| 57 | 38504 | 40130 |
| 58 | 38562 | 40245 |
| 59 | 38643 | 40328 |
| 60 | 38658 | 40413 |
| 61 | 38740 | 40504 |
| 62 | 38782 | 40559 |
| 63 | 38839 | 40606 |
| 64 | 38887 | 40674 |
| 65 | 38981 | 40784 |
| 66 | 39036 | 40830 |
| 67 | 39101 | 40884 |
| 68 | 39201 | 40997 |
| 69 | 39208 | 41044 |
| 70 | 39291 | 41075 |
| 71 | 39353 | 41109 |
| 72 | 39400 | 41211 |
| 73 | 39445 | 41200 |
| 74 | 39521 | 41269 |
| 75 | 39548 | 41261 |

## Nota — escala do "raw" vs valores de campo (comparação com testes do cliente)

Os valores desta tabela são o **raw da chip**, capturados via `[ADCDBG] chip=ADS1248 ch=... raw=...`
(`CONFIG_ADC_DEBUG_SERIAL=y`, ligado só em builds de debug). Em operação normal esse raw
**não é o que aparece no log** — `main/ampoule_test.cpp:748` faz `sensor = sensor / 100` antes de
imprimir `"Valor recebido do sensor"` e guardar a amostra usada no cálculo de positivo/negativo.
Ao comparar com logs de campo (ex.: `2026-08-12d-cliente-corridas-positiva-negativa-com4.md`, que
não tem `CONFIG_ADC_DEBUG_SERIAL` ligado e só mostra o valor já dividido), dividir este raw por 100
antes de comparar — senão parece uma diferença de ~100x que não existe de verdade.

## Ambiente

- Firmware: mesmo binário gravado na sessão anterior de hoje (nenhum build/flash novo nesta rodada).
- Monitor serial: `py -m serial.tools.miniterm COMxx 115200 --raw` em background nas duas portas, saída redirecionada pra `historico/Testes/`.
