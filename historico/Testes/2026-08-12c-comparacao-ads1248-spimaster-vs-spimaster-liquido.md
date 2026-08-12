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

## Valores brutos (raw) — todas as 75 leituras, na ordem capturada

| # | spi_master (COM20) | spi_master (COM22) |
|---|---|---|
| 1 | 51021 | 52424 |
| 2 | 50648 | 52156 |
| 3 | 50395 | 51873 |
| 4 | 50171 | 51628 |
| 5 | 49879 | 51454 |
| 6 | 49600 | 51103 |
| 7 | 49386 | 51079 |
| 8 | 49204 | 50894 |
| 9 | 49034 | 50898 |
| 10 | 48956 | 50787 |
| 11 | 48859 | 50763 |
| 12 | 48597 | 50362 |
| 13 | 48520 | 50248 |
| 14 | 48485 | 50275 |
| 15 | 48473 | 50249 |
| 16 | 48394 | 50219 |
| 17 | 48374 | 50145 |
| 18 | 48346 | 50195 |
| 19 | 48261 | 50042 |
| 20 | 48238 | 49990 |
| 21 | 48227 | 49867 |
| 22 | 48102 | 49699 |
| 23 | 48190 | 49669 |
| 24 | 48082 | 49594 |
| 25 | 47955 | 49531 |
| 26 | 47954 | 49473 |
| 27 | 47984 | 49401 |
| 28 | 47921 | 49364 |
| 29 | 47887 | 49384 |
| 30 | 47906 | 49331 |
| 31 | 47991 | 49452 |
| 32 | 48074 | 49449 |
| 33 | 48089 | 49485 |
| 34 | 48141 | 49476 |
| 35 | 48236 | 49590 |
| 36 | 48416 | 49705 |
| 37 | 48617 | 49917 |
| 38 | 48788 | 50094 |
| 39 | 48936 | 50307 |
| 40 | 49071 | 50375 |
| 41 | 49177 | 50412 |
| 42 | 49311 | 50620 |
| 43 | 49454 | 50683 |
| 44 | 49552 | 50803 |
| 45 | 49669 | 50857 |
| 46 | 49724 | 50960 |
| 47 | 49848 | 51037 |
| 48 | 49935 | 51093 |
| 49 | 50038 | 51121 |
| 50 | 50116 | 51170 |
| 51 | 50239 | 51272 |
| 52 | 50339 | 51492 |
| 53 | 50548 | 51758 |
| 54 | 50869 | 52052 |
| 55 | 51256 | 52484 |
| 56 | 51685 | 52938 |
| 57 | 52166 | 53472 |
| 58 | 52550 | 53873 |
| 59 | 52832 | 54174 |
| 60 | 53082 | 54347 |
| 61 | 53320 | 54593 |
| 62 | 53521 | 54677 |
| 63 | 53667 | 54815 |
| 64 | 53836 | 54937 |
| 65 | 53906 | 55028 |
| 66 | 53977 | 55039 |
| 67 | 54067 | 55129 |
| 68 | 54146 | 55217 |
| 69 | 54281 | 55296 |
| 70 | 54427 | 55518 |
| 71 | 54567 | 55560 |
| 72 | 54731 | 55836 |
| 73 | 54958 | 56045 |
| 74 | 55210 | 56719 |
| 75 | 56212 | 57593 |

**Padrão observado:** ao contrário dos testes anteriores (queda inicial + recuperação parcial), aqui as duas curvas caem só nas primeiras ~30 leituras e depois sobem continuamente até o fim do ciclo, terminando acima do valor inicial — bate com a variação percentual positiva (curva de subida, não de descida). As duas placas seguem o mesmo formato de curva, praticamente em fase uma com a outra, reforçando que a diferença agora é só de escala (~2,75% na média), não de comportamento.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`, fullclean+reconfigure+build) — reflash da COM20 pra spi_master, `main/light_sensor_ads1248.cpp` já estava no estado commitado (`658e662`), nenhuma mudança de código necessária.
- Flash: `.\flash_Lummina4EdAdj.ps1 -Port COM20`.
- Monitor serial: `py -m serial.tools.miniterm COMxx 115200 --raw` em background nas duas portas, saída redirecionada pra `historico/Testes/`.
