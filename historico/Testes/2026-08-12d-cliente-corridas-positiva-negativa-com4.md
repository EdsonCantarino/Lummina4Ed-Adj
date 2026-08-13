# Teste do cliente — 2 corridas de 4 ampolas (COM4), uma positivada e outra negativada

**Data:** 12/08/2026 (log recebido e analisado em 13/08/2026)
**Origem:** log serial enviado pelo cliente Maxximed, porta COM4, unidade de campo (não é bancada ECK).
**Objetivo:** cliente relatou ter rodado ontem duas corridas de 4 ampolas cada — uma dando positivo nas 4 e outra dando negativo nas 4 — e pediu análise do log pra confirmar o comportamento. Log analisado ponta a ponta: sem erros, reinícios ou crashes durante nenhuma das corridas; equipamento operou o tempo todo no mesmo modo de temperatura (~60°C, range 53–67°C).

**Log bruto:** `monitor_com4_cliente_corridas_pos_neg_12_08.log`.

## Resultado — Corrida 1 (16:54:43–17:00:11), todas POSITIVADAS

| Ampola | Início | Fim | Média | Variação % | Positive Percentage | Resultado |
|---|---|---|---|---|---|---|
| 1 | 16:54:43 | 16:59:44 | 356 | +131,26 | 10,0% | POSITIVADA |
| 2 | 16:54:54 | 16:59:54 | 328 | +121,76 | 10,0% | POSITIVADA |
| 3 | 16:55:05 | 17:00:04 | 373 | +146,15 | 10,0% | POSITIVADA |
| 4 | 16:55:06 | 17:00:07 | 410 | +160,90 | 10,0% | POSITIVADA |

## Resultado — Corrida 2 (17:02:24–17:07:31), todas NEGATIVADAS

| Ampola | Início | Fim | Média | Variação % | Positive Percentage | Resultado |
|---|---|---|---|---|---|---|
| 1 | 17:02:24 | 17:07:24 | 219 | -14,54 | 10,0% | NEGATIVADA |
| 2 | 17:02:24 | 17:07:24 | 193 | -11,70 | 10,0% | NEGATIVADA |
| 3 | 17:02:25 | 17:07:25 | 227 | -13,16 | 10,0% | NEGATIVADA |
| 4 | 17:02:26 | 17:07:26 | 229 | -14,24 | 10,0% | NEGATIVADA |

## Conclusão

Separação bem larga entre os dois grupos (+121% a +161% na corrida positiva vs -12% a -15% na negativa), contra um limiar configurado de apenas 10% — nenhum dos 8 resultados ficou perto da borda de decisão. Padrão bate com o esperado fisicamente: na corrida positiva as leituras sobem ~2,2x da primeira pra última amostra (crescimento do indicador biológico, sem esterilização efetiva); na corrida negativa as leituras caem ou ficam estáveis (sem metabolismo, esterilização efetiva). Não foi encontrado nenhum erro de impressora, RTC, Wi-Fi, watchdog ou reinício durante as duas corridas — os únicos avisos no log são de impressora ausente no boot, esperados nessa unidade sem impressora conectada.

Cada ampola aparece com o bloco de resultado impresso duas vezes seguidas no log (mesma média, mesmo resultado) — comportamento esperado de `print_test_result()` (chamado uma vez perto do fim do teste e de novo na finalização), não é bug.

## Valores brutos (AD) — 10 amostras por ampola, na ordem capturada

### Corrida 1 — POSITIVADA

| # | Ampola 1 | Ampola 2 | Ampola 3 | Ampola 4 |
|---|---|---|---|---|
| 1 | 214 | 208 | 222 | 232 |
| 2 | 216 | 204 | 216 | 226 |
| 3 | 214 | 202 | 213 | 223 |
| 4 | 214 | 202 | 213 | 225 |
| 5 | 217 | 204 | 215 | 232 |
| 6 | 479 | 432 | 506 | 574 |
| 7 | 488 | 444 | 520 | 585 |
| 8 | 498 | 455 | 526 | 589 |
| 9 | 508 | 454 | 546 | 606 |
| 10 | 513 | 477 | 558 | 615 |
| **Média** | **356** | **328** | **373** | **410** |

### Corrida 2 — NEGATIVADA

| # | Ampola 1 | Ampola 2 | Ampola 3 | Ampola 4 |
|---|---|---|---|---|
| 1 | 258 | 223 | 267 | 271 |
| 2 | 242 | 210 | 249 | 253 |
| 3 | 234 | 202 | 239 | 243 |
| 4 | 226 | 197 | 233 | 237 |
| 5 | 223 | 194 | 228 | 232 |
| 6 | 202 | 181 | 211 | 212 |
| 7 | 202 | 181 | 211 | 212 |
| 8 | 202 | 181 | 211 | 212 |
| 9 | 202 | 181 | 211 | 212 |
| 10 | 203 | 182 | 212 | 212 |
| **Média** | **219** | **193** | **227** | **229** |

**Padrão observado:** na corrida positiva as 4 curvas começam estáveis em ~210–230 e saltam pras amostras 6–10 (transição visível entre a amostra 5 e a 6, salto de ~2x), consistente com o indicador biológico reagindo/colorindo durante o teste. Na corrida negativa as 4 curvas caem de forma gradual e continua até estabilizar nas últimas amostras, sem salto — sem sinal de reação.

## Ambiente

- Unidade de campo do cliente (Maxximed), não é bancada de desenvolvimento ECK.
- Firmware em uso não identificado a partir do log (log de campo, sem linha de versão capturada no trecho relevante).
- Log capturado via ferramenta própria do cliente/campo (não é o `tools/serial_logger.py` da ECK, que só foi entregue depois desta sessão de log).
