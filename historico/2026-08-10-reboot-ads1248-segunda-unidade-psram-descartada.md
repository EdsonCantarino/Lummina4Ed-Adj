# Investigação — reboot intermitente ADS1248, segunda unidade + teoria da PSRAM descartada

**Data:** 10/08/2026
**Branch:** `feature/config-web`
**Status:** causa raiz **ainda não identificada**. Teoria forte (conflito elétrico IO35/36/37 com a PSRAM Octal do módulo N8R8) foi testada de forma decisiva e **descartada**. Achado colateral real (bug de loop sem delay no `check_temperature_task`) foi corrigido. Próximo passo recomendado: osciloscópio no pino EN/RESET do módulo, mirando nos instantes já conhecidos (~7-20s de boot).

## Contexto

Continuação de `historico/2026-08-07-reboot-intermitente-placa-ads1248.md`. Plano era testar uma segunda unidade física da placa ADS1248 (COM20) pra ver se o mesmo reset intermitente aparecia nela também.

## Reproduzido na segunda unidade

Mesma assinatura exata da primeira placa: `RTC_SW_SYS_RST` ("Software reset digital core"), zero linha de log entre o boot anterior e o próximo banner ROM, nunca grava coredump, nunca bate em nenhum dos 4 `RESTART_ID` já instrumentados (`printer.cpp`, `app_httpd.cpp` x2, `light_sensor.cpp`). Reproduz em loop rápido logo após ligar (tipicamente resets a cada 7-30s), às vezes intercalado com corridas mais longas (chegou a rodar 150s-340s estável antes de resetar de novo, sem padrão claro de "estabiliza de vez").

Osciloscópio na alimentação: nada de anormal (também bate com o motivo do reset não ser brownout).

Impressora conectada ou não: sem diferença no padrão.

## Achado de timing (novo nesta sessão)

Os resets NÃO caem em tempos aleatórios — se agrupam quase no mesmo milissegundo (contado desde o boot, campo `I (ms)` do log) em dezenas de ciclos diferentes: ~7389-7409ms (bem na hora do `PRINTER_DRIVER: Registering Client` / enumeração USB), depois ~10079-10089ms, ~13179ms, ~17359-17369ms, ~20199ms, etc. (múltiplos aproximados de ~3s, batendo com o tick periódico do `HEATER: AGUARDE`). Isso indica proximidade determinística com atividade periódica do firmware, não puramente ruído térmico aleatório.

## Teoria investigada e DESCARTADA: conflito IO35/36/37 vs PSRAM Octal (N8R8)

### A teoria

O datasheet oficial (`esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf`, footnote "b" da Tabela 3-1, pág. 12) diz: *"For modules with Octal SPI PSRAM, i.e., modules embedded with ESP32-S3R8 or ESP32-S3R16V, pins IO35, IO36, and IO37 are connected to the Octal SPI PSRAM and are not available for other uses."*

O schematic da placa ADS1248 (`Imagens\ADS ESp32 pinout.png`) usa exatamente esses 3 pinos: **AQ (Aquecedor/heater) = IO35, Buzzer = IO36, TEMP (sensor DS18B20 de calibração) = IO37**. Achado via `CONFIG_HEATER_GPIO=35` (Kconfig, `main/Kconfig.projbuild`) e `CONFIG_DS18X20_ONEWIRE_GPIO=37`/`CONFIG_BUZZER_GPIO=36`.

Verificado via `esptool --port COM20 chip-id`: a unidade ADS1248 **e** a unidade CS5534 (mesmo teste na mesma porta depois de trocar a placa) reportam ambas `Features: ... Embedded PSRAM 8MB (AP_3v3)` — mesmo variante de módulo (`ESP32-S3-WROOM-1-N8R8`) nas duas placas, mesmo pinout no schematic (`Imagens\CS ESP32pinout.png` é idêntico ao da ADS nesses 3 pinos). Ou seja, a "violação" existe igualmente nas duas placas — não explica por que só a ADS1248 falha, mas não descarta ser fator latente combinado com algo específico da ADS1248 (SPI bit-bang, layout).

Contra-argumento levantado (usuário + segunda opinião do Gemini): mesmo com `CONFIG_SPIRAM` desabilitado no `sdkconfig` (confirmado: está desligado), a ligação entre o pino externo e o die da PSRAM é feita por **bond wire dentro do encapsulamento do chip/módulo** (SiP), não por mux de software — o firmware só decide se o *lado ESP32-S3* daquele pino vira GPIO ou controlador SPI, mas não desconecta o *outro lado* (a PSRAM) do mesmo nó elétrico. Ou seja, "funciona normalmente na maior parte do tempo" não é evidência contra a teoria (o sintoma é justamente intermitente).

### Teste decisivo (elimina a variável por completo)

1. Migrados os 3 sinais pra pinos livres via Kconfig/sdkconfig: `CONFIG_HEATER_GPIO=42`, `CONFIG_BUZZER_GPIO=38`, `CONFIG_DS18X20_ONEWIRE_GPIO=41` (pinos escolhidos por estarem sem função no schematic e longe de pinos de strapping de boot: IO0/3/45/46/47/48 evitados de propósito).
2. Corrigido efeito colateral: sem sensor DS18B20 fisicamente religado ainda, `check_temperature_task` (`main/temperature.cpp`) entrava num **loop sem delay** quando `sensor_count == 0` (bug pré-existente, não relacionado à PSRAM: `continue` direto sem `vTaskDelay`), disparando `task_wdt` a cada 5s exatos (log mostra backtraces reais pela primeira vez na investigação, mas esse watchdog específico não reseta o sistema porque `CONFIG_ESP_TASK_WDT_PANIC` está desligado). Corrigido temporariamente: quando nenhum sensor é encontrado, força `last_temperature = 35.0f` e segue com `vTaskDelay(LOOP_DELAY_MS)` — precisa de solução definitiva depois (ver pendências).
3. Testado só a migração do heater pro IO14 primeiro (isolado) — resultado ambíguo (variou de resets a cada 7-13s até corridas de 150-340s estável).
4. Testado com o **usuário cortando fisicamente as 3 trilhas** originais (IO35/36/37 completamente desconectadas do mundo externo, único vínculo restante é o bond wire interno do chip com a PSRAM) + firmware gravado com os 3 sinais totalmente migrados pros pinos 42/41/38. **Resultado: reset continuou no mesmo padrão característico (resets a cada 8-28s, ritmo idêntico ao de antes).**

**Conclusão: a teoria da PSRAM está descartada.** Eliminamos essa variável por completo (fisicamente, não só por firmware) e o sintoma não mudou.

### Também testado e descartado nesse processo
- Trava em nível baixo do IO35/36/37 via firmware (`gpio_set_level(...,0)`) — implementada e depois **revertida** (o usuário ia cortar as trilhas de qualquer forma, tornando a trava desnecessária; também levantamos que travar ativamente pode ser pior que flutuar, se a PSRAM algum dia tentar dirigir o mesmo nó).
- SPI bit-bang do ADS1248 (`light_sensor_ads1248.cpp`) não desabilita interrupções (sem `taskENTER_CRITICAL`/`portDISABLE_INTERRUPTS`) — não é uma fonte óbvia de estouro do interrupt watchdog.
- Timeout de DRDY do ADS1248 (`read_channel_value`) já tem timeout com `vTaskDelay` a cada 10ms — não é um loop travado.

## Estado do repo ao fim da sessão (NADA COMMITADO)

- `sdkconfig`: `CONFIG_HEATER_GPIO=42` (era 35), `CONFIG_BUZZER_GPIO=38` (era 36), `CONFIG_DS18X20_ONEWIRE_GPIO=41` (era 37). **Diverge do padrão de produção** — não fazer merge sem decidir se essa realocação de pino vira permanente (ver pendências).
- `main/temperature.cpp`: bug real corrigido (loop sem delay quando sensor ausente) + hack temporário de diagnóstico (força 35.0°C quando não acha o DS18B20) — **precisa de solução definitiva antes de qualquer release** (ver pendências).
- `main/main.cpp`: sem mudança líquida (trava em nível baixo foi adicionada e depois revertida).
- COM20 está com esse firmware de diagnóstico gravado (pinos migrados, sem trava, sem sensor DS18B20 fisicamente religado ainda).
- Placa ADS1248 física: as 3 trilhas originais (AQ/Buzzer/TEMP → IO35/36/37) foram **cortadas fisicamente**. Heater, buzzer e leitura de calibração DS18B20 **não funcionam** até fazer os jumpers pros pinos novos (42/38/41).

## Pendente pra próxima sessão

1. **Osciloscópio no pino EN/RESET do módulo** (`EN-PROG`, pino 3 do `ESP32-S3-WROOM-1-N8R8`) — item que já estava pendente desde 07/08, agora mais viável porque sabemos os instantes aproximados (~7-20s de boot) pra mirar o disparo do osciloscópio. Se o pino cair fisicamente nesse momento, confirma reset externo/hardware; se ficar estável, aponta pra algo interno ao chip (ex.: RTC Super Watchdog, que pode resetar sem passar pelo panic handler normal).
2. Fazer os jumpers físicos definitivos: AQ→IO42, Buzzer→IO38, TEMP(DS18B20)→IO41 — mesmo a teoria da PSRAM tendo sido descartada como causa do reset, os pinos originais são uma violação documentada do datasheet e vale manter a migração de qualquer forma numa revisão de placa futura.
3. Decidir solução definitiva pro sensor DS18B20 de calibração ausente em `temperature.cpp` (hoje está mascarado forçando 35.0°C) — religar o sensor fisicamente no IO41 é o caminho mais simples.
4. Reverter/decidir sobre a numeração de pino no `sdkconfig` antes de commitar (35/36/37 vs 42/38/41) — se migrar de vez, também atualizar os defaults no `main/Kconfig.projbuild` (hoje aponta pra 35/36/37).
5. Considerar investigar o RTC Super Watchdog / medição direta do pino EN como próxima hipótese, já que praticamente todo caminho de crash de software conhecido (panic, task/int watchdog com print, os 4 `esp_restart()` instrumentados, coredump) foi eliminado por teste direto.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`).
- Flash: `.\flash_Lummina4EdAdj.ps1 -Port COM20` (usa `py -m esptool`).
- Monitor serial: `py -m serial.tools.miniterm COM20 115200 --raw` em background, saída redirecionada pra arquivo, filtrada por reset real (linha completa do banner ROM `rst:0x...,boot:0x19...`) — evitar filtrar só por "Software reset digital core"/"Vbat power on reset", que também aparecem dentro de um dump de histórico de reset armazenado na NVS (até 20 entradas antigas relidas a cada boot), gerando falso positivo de contagem de eventos.
- `esptool --port COM20 chip-id` só conecta se a placa não tiver circuito de auto-reset/boot funcionando — pode precisar segurar BOOT/IO0 e apertar RESET manualmente.
