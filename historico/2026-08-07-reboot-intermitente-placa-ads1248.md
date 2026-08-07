# Investigação — reboot intermitente na placa nova ADS1248 (bring-up)

**Data:** 07/08/2026
**Branch:** `feature/config-web`
**Status:** causa raiz **não identificada** ainda. Praticamente tudo em firmware e alimentação externa foi eliminado por teste direto. Indícios fortes de defeito físico intermitente (solda fria) sensível à temperatura da própria placa. Retomar segunda-feira com uma segunda unidade ADS1248 pra comparar.

## Contexto

Continuação do bring-up da placa nova ADS1248 (ver `historico/2026-08-07-comparacao-ads1248-vs-cs5534.md` pro teste comparativo de leitura do AD feito mais cedo hoje, que funcionou bem). Depois desse teste, ao esfriar/religar a unidade em COM20, apareceu um **reboot intermitente** que não existia antes.

## Sintoma

Reset automático (`RTC_SW_SYS_RST` / "Software reset digital core"), tipicamente entre ~7s e ~20s de boot, sem nenhuma mensagem de log entre a última linha normal e o banner do ROM bootloader do próximo boot. Comportamento **inconsistente**: às vezes reseta em loop rápido por 20+ ciclos seguidos, às vezes fica estável rodando por bastante tempo (teve boot de 60s+ sem resetar). Não é um padrão de "reseta N vezes e depois resolve pra sempre" — dados da sessão mostram resets em praticamente todo ciclo por longos trechos.

**Só acontece na placa ADS1248.** Mesmo firmware base testado na placa CS5534 (COM21) não reiniciou nenhuma vez em teste equivalente.

## O que foi eliminado (testado diretamente, não só lido no código)

1. **Watchdog de reinício de impressora já corrigido antes** — não é isso (nenhuma mensagem "Erro de impressora detectado" nem contagem de carência apareceu nesta sessão).
2. **As 4 chamadas de `esp_restart()` do app** — instrumentadas com um ID único (`RESTART_ID=1..4`) + `fflush(stdout)` + delay de 100ms antes de cada uma (`main/printer.cpp`, `main/app_httpd.cpp` x2, `main/light_sensor.cpp`). Reflashado e testado: **nenhum ID apareceu** em vários resets. Nenhuma das 4 é a causa.
3. **`abort()` no `client_event_cb()` do driver USB** (`components/usb_printer/usb_class_driver.cpp`, `default:` do switch, só tratava `NEW_DEV`/`DEV_GONE`) — trocado por log do valor do evento em vez de abortar. Reflashado: **continuou resetando igual**, sem nunca logar "evento inesperado". Não é esse abort().
4. **Crash/panic/assert genérico** — `CONFIG_ESP_SYSTEM_PANIC_REBOOT_DELAY_SECONDS` de 0 pra 3s (garantir tempo de flush de qualquer texto de pânico) e **coredump-to-flash habilitado** (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`, partição `coredump` confirmada presente: `esp_core_dump_flash: Found partition 'coredump' @ 520000 65536 bytes`). Testado em **~20 resets consecutivos direto no hardware** — nenhum gravou coredump (`coredump: No coredump in flash` sempre no boot seguinte). Isso descarta crash/panic/assert de verdade: o ESP-IDF sempre grava coredump antes de reiniciar quando habilitado, se fosse esse o caso.
5. **MOSFET/corrente do aquecedor** (defeito de hardware já isolado antes, ver histórico de bring-up anterior) — resistência de aquecimento fisicamente desconectada, continuou resetando igual. Não é isso.
6. **Fonte de alimentação** — trocada por uma fonte diferente, comportamento idêntico nas duas. Não é fonte externa marginal.
7. **`heater.cpp`/`heater_fail.cpp`** — não têm `esp_restart`/`abort`/`assert` em lugar nenhum (conferido por leitura completa).

## Observação física do usuário (chave pra próxima hipótese)

Sem a resistência de aquecimento conectada, o padrão de reset muda com o tempo de uso: mais frequente logo depois de ligar (placa fria), tende a sumir depois que a placa fica ligada/rodando um tempo (autoaquecimento dos próprios componentes, não da resistência). Isso é consistente com **solda fria ou contato marginal sensível à temperatura** — contato ruim a frio, melhora com a dilatação térmica do próprio componente/placa. Explica também por que o teste de comparação AD (5 minutos, bem-sucedido) rodou limpo: a unidade já estava ligada tempo suficiente antes daquele teste começar.

Também descartado por inspeção física das duas PCBs: a área do ADS1248 fica separada da área do USB no layout — enfraquece a hipótese de ruído elétrico USB↔ADS1248 (mas não elimina 100%).

## Pendente pra próxima sessão

1. **Testar com uma segunda unidade ADS1248** (plano do usuário pra segunda-feira) — se o mesmo comportamento aparecer nela também, aponta pra problema sistêmico de design/lote, não defeito isolado de solda numa placa só.
2. Se der pra reproduzir sob demanda: aquecer levemente (dedo ou soprador térmico em temperatura baixa) uma área suspeita do circuito de reset/EN ou do regulador de tensão enquanto a placa está resetando em loop, e ver se os resets param — ajuda a localizar o ponto físico exato.
3. Medir o pino EN/RESET do módulo (pino 3 do `ESP32-S3-WROOM-1-N8R8` conforme o schematic, net `EN-PROG`) com multímetro/osciloscópio no momento do reset — se cair fisicamente, confirma reset externo; se ficar estável, aponta mais pro ESP-IDF interno (não verificado ainda, já que o SDK vive dentro da imagem Docker, não no repo).
4. Inspeção visual da solda com lupa, focando componentes de montagem manual (mesmo tipo de suspeita já usada antes pro MOSFET Q2 do aquecedor).

## Estado do repo ao fim da sessão (nada commitado ainda)

- `main/printer.cpp`, `main/app_httpd.cpp` (x2), `main/light_sensor.cpp`: instrumentação `RESTART_ID` nas 4 chamadas de `esp_restart()`.
- `components/usb_printer/usb_class_driver.cpp`: `abort()` trocado por log no `default:` do `client_event_cb()` — essa parte parece uma melhoria legítima independente da causa raiz final (evita derrubar o equipamento por um evento USB inesperado em campo), vale considerar manter mesmo depois de achar a causa real.
- `sdkconfig`: `CONFIG_ADC_CHIP_ADS1248=y` (trocado de volta pra ADS1248 no fim da sessão), `CONFIG_ESP_SYSTEM_PANIC_REBOOT_DELAY_SECONDS=3` (era 0), `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` + `CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH=y` (eram `_TO_NONE`) — avaliar se o coredump habilitado vale ficar permanente (ajuda a diagnosticar qualquer crash futuro em campo) ou se é só pra esta investigação.
- COM20 está fisicamente com esse firmware de diagnóstico gravado (ADS1248 + instrumentação + coredump habilitado) - útil manter assim até a próxima sessão pra continuar testando sem precisar regravar.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`).
- Flash: `py -m esptool`, COM20.
- Monitor serial: `py -m serial.tools.miniterm COM20 115200 --raw` em background, saída redirecionada pra arquivo e filtrada por `grep`.
