# Teste comparativo — ADS1248 spi_master (COM20) vs ADS1248 spi_master (COM22), ampola COM líquido

**Data:** 12/08/2026
**Branch:** `feature/config-web`
**Objetivo:** nos dois testes anteriores com ampola vazia e com líquido (bit-bang COM20 vs spi_master COM22), a "Variação Percentual" do spi_master ficou sistematicamente menor que a do bit-bang (-7,66% vs -10,48% vazia; -1,77% vs -6,76% líquido). Pra separar se isso vem do **driver** ou de **variação entre as duas placas físicas**, a COM20 foi reflashada com spi_master (mesmo binário da COM22) e o teste com líquido foi repetido — agora as duas placas rodam o mesmo driver.

## Placas

- **COM20**: ADS1248, driver **spi_master** (reflashada nesta sessão — antes rodava bit-bang).
- **COM22**: ADS1248, driver **spi_master** (sem alteração).

Mesma ampola com líquido usada no teste anterior (`2026-08-12b-...-liquido.md`), reinserida pela 2ª vez nesta sessão.

**Logs brutos:** `monitor_com20_spimaster_liquido_12_08.log`, `monitor_com22_spimaster_liquido2_12_08.log`.

## Resultado

| | ADS1248 spi_master (COM20) | ADS1248 spi_master (COM22) |
|---|---|---|
| Leituras no ciclo | 75 | 75 |
| Raw mínimo | 47887 | 49331 |
| Raw máximo | 56212 | 57593 |
| Raw médio | 50328,3 | 51714,2 |
| Positive Percentage | 40,0% | 40,0% |
| Variação percentual | **+9,41** | **+8,56** |
| Resultado final (Is Positive) | 0 (negativo) | 0 (negativo) |

## Conclusão

Com os dois lados rodando o mesmo driver (spi_master), a Variação Percentual ficou muito mais próxima entre as placas (+9,41 vs +8,56, diferença de ~0,85 ponto) do que no teste anterior com drivers diferentes (-1,77 vs -6,76, ~4x de diferença relativa, mesma ampola/líquido). Isso aponta que o **driver era o fator dominante** na discrepância de variação vista nos testes bit-bang vs spi_master de hoje — não (só) diferença de hardware entre as duas placas físicas.

## Observações à parte (fora do escopo da pergunta original)

1. **Sinal da variação inverteu**: nos dois testes anteriores com essa ampola (vazia e líquido, ambos hoje) a variação foi sempre negativa (curva caindo ao longo do ciclo). Nesta rodada ficou positiva nas duas placas (curva subindo). Não investigado.
2. **Raw médio subiu bastante**: de ~38-40k (teste anterior, mesma ampola com líquido) para ~50-52k nesta rodada — terceiro uso da mesma ampola hoje. Possível reação do reagente progredindo com exposição UV acumulada entre os testes, ou efeito térmico acumulado no equipamento/ampola. Não investigado — só registrado como pista pra quem for olhar isso depois.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`, fullclean+reconfigure+build) — reflash da COM20 pra spi_master, `main/light_sensor_ads1248.cpp` já estava no estado commitado (`658e662`), nenhuma mudança de código necessária.
- Flash: `.\flash_Lummina4EdAdj.ps1 -Port COM20`.
- Monitor serial: `py -m serial.tools.miniterm COMxx 115200 --raw` em background nas duas portas, saída redirecionada pra `historico/Testes/`.
