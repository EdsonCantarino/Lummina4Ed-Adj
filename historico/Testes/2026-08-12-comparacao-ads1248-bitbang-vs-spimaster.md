# Teste comparativo — ADS1248 bit-bang (COM20) vs ADS1248 spi_master (COM22)

**Data:** 12/08/2026
**Branch:** `feature/config-web`
**Objetivo:** isolar se a diferença de escala observada em 11/08 (ADS1248 spi_master vs CS5534, ~11,9x) vem do **driver** (spi_master vs bit-bang) ou de **condição física** do teste (ampola, iluminação, intensidade do LED UV) — pendência registrada em `historico/Teste_ADS_CS_SPI_Master_11_08_2026_16_48.md`. Esse teste usa o **mesmo chip** (ADS1248) nas duas placas, só o driver muda — controle melhor que comparar contra o CS5534 (chip diferente).

## Placas

- **COM20**: placa ADS1248, driver **bit-bang** (código do commit `191f7e1`, aplicado temporariamente na árvore de trabalho só para esta gravação — não commitado; `main/light_sensor_ads1248.cpp` foi revertido para o `spi_master` commitado, `658e662`, logo depois do flash).
- **COM22**: placa ADS1248, driver **spi_master** (firmware já gravado na sessão de 11/08 à tarde, pós-fix do bus-lock em `ads1248_rdata()`).

Ampolas inseridas ao mesmo tempo na cavidade 1 das duas placas, ciclo de 5 minutos (300s).

**Logs brutos:** `historico/Testes/monitor_com20_bitbang_comparativo_12_08.log`, `historico/Testes/monitor_com22_spimaster_comparativo_12_08.log`.

## Resultado

| | ADS1248 bit-bang (COM20) | ADS1248 spi_master (COM22) |
|---|---|---|
| Leituras no ciclo | 75 | 68 |
| Raw mínimo | 26971 | 29892 |
| Raw máximo | 32655 | 32468 |
| Raw médio | 28105,5 | 30817,0 |
| Positive Percentage | 40,0% | 40,0% |
| Variação percentual | -10,48% | -7,66% |
| Resultado final (Is Positive) | 0 (negativo) | 0 (negativo) |

Proporção spi_master/bit-bang: **~1,10x** (média 30817 vs 28105,5) — bem mais próxima que a proporção ADS1248/CS5534 de 11/08 (~11,9x, chips diferentes). Classificação final idêntica (negativo, 40%) nas duas capturas. Zero reset, zero panic/abort em qualquer uma das duas capturas.

## Conclusão

Com o mesmo chip ADS1248 nas duas placas, bit-bang e spi_master ficam na mesma ordem de grandeza (~10% de diferença), não a mesma ordem de grandeza de diferença vista contra o CS5534. Isso aponta que a diferença grande de escala do teste de 11/08 (ADS1248 spi_master vs CS5534, ~11,9x) é predominantemente **característica analógica do chip** (ADS1248 vs CS5534), não um artefato do driver spi_master. Os ~10% de diferença entre bit-bang e spi_master aqui podem vir de timing de amostragem ligeiramente diferente entre os dois drivers (nº de leituras diferente: 75 vs 68 no mesmo ciclo de 300s) ou de leve variação física entre sessões (mesma ressalva já registrada em 11/08 sobre condição do teste variar entre sessões).

## Valores brutos (raw) — todas as leituras, na ordem capturada

| # | bit-bang (COM20) | spi_master (COM22) |
|---|---|---|
| 1 | 32655 | 32468 |
| 2 | 31642 | 32145 |
| 3 | 30851 | 32237 |
| 4 | 30456 | 32151 |
| 5 | 30170 | 32042 |
| 6 | 30048 | 32001 |
| 7 | 29755 | 31878 |
| 8 | 29588 | 31784 |
| 9 | 29405 | 31724 |
| 10 | 29285 | 31664 |
| 11 | 29186 | 31531 |
| 12 | 29005 | 31419 |
| 13 | 28926 | 31335 |
| 14 | 28816 | 31196 |
| 15 | 28687 | 31132 |
| 16 | 28602 | 31081 |
| 17 | 28473 | 31056 |
| 18 | 28387 | 30988 |
| 19 | 28289 | 30968 |
| 20 | 28193 | 30688 |
| 21 | 28071 | 30272 |
| 22 | 27949 | 30245 |
| 23 | 27862 | 30830 |
| 24 | 27824 | 30840 |
| 25 | 27807 | 30814 |
| 26 | 27766 | 30788 |
| 27 | 27743 | 30757 |
| 28 | 27638 | 30737 |
| 29 | 27615 | 30622 |
| 30 | 27606 | 30583 |
| 31 | 27590 | 30571 |
| 32 | 27536 | 30556 |
| 33 | 27505 | 30530 |
| 34 | 27486 | 30517 |
| 35 | 27493 | 29892 |
| 36 | 27471 | 29902 |
| 37 | 27445 | 29910 |
| 38 | 27454 | 29930 |
| 39 | 27452 | 29965 |
| 40 | 27457 | 29997 |
| 41 | 26971 | 30043 |
| 42 | 27350 | 30015 |
| 43 | 27334 | 30095 |
| 44 | 27347 | 30066 |
| 45 | 27331 | 30126 |
| 46 | 27357 | 30220 |
| 47 | 27355 | 30249 |
| 48 | 27389 | 30334 |
| 49 | 27362 | 30522 |
| 50 | 27392 | 30515 |
| 51 | 27394 | 30564 |
| 52 | 27423 | 30583 |
| 53 | 27425 | 30575 |
| 54 | 27475 | 30615 |
| 55 | 27483 | 30632 |
| 56 | 27496 | 30668 |
| 57 | 27549 | 30687 |
| 58 | 27556 | 30704 |
| 59 | 27583 | 30769 |
| 60 | 27604 | 30743 |
| 61 | 27626 | 30797 |
| 62 | 27623 | 30848 |
| 63 | 27670 | 30848 |
| 64 | 27691 | 30856 |
| 65 | 27705 | 30888 |
| 66 | 27697 | 30907 |
| 67 | 27713 | 30947 |
| 68 | 27764 | 30997 |
| 69 | 27805 | — |
| 70 | 27796 | — |
| 71 | 27816 | — |
| 72 | 27869 | — |
| 73 | 27889 | — |
| 74 | 27942 | — |
| 75 | 27945 | — |

**Padrão observado:** as duas curvas seguem o mesmo formato — queda acentuada nas primeiras ~20-25 leituras (estabilização inicial/exposição UV), depois patamar com variação pequena. spi_master fica consistentemente ~10% acima do bit-bang durante todo o platô, sem cruzar nem inverter — diferença sistemática de escala, não ruído aleatório. COM22 (spi_master) completou só 68 leituras contra 75 do COM20 (bit-bang) no mesmo intervalo de 300s — spi_master roda o ciclo de leitura um pouco mais devagar nesse teste.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`, fullclean+reconfigure+build).
- Flash: `.\flash_Lummina4EdAdj.ps1 -Port COM20`.
- Monitor serial: `py -m serial.tools.miniterm COMxx 115200 --raw` em background nas duas portas, saída redirecionada pra arquivo.
- **Nota de porta:** a segunda placa (spi_master) apareceu em **COM22**, não COM21 como nas sessões anteriores — mudou de porta USB física entre sessões. Conferir sempre com `python -m serial.tools.list_ports -v` antes de assumir a porta de sessões passadas.
