# Investigação — reboot intermitente ADS1248: causa raiz identificada e reparada (regulador U2)

**Data:** 11/08/2026
**Branch:** `feature/config-web`
**Status:** **RESOLVIDO.** Causa raiz identificada em nível de componente: regulador chaveado **U2 (LM2576D2T-3.3)** defeituoso, não sustentava o pico de corrente transitório do boot (WiFi AP + USB Host inicializando juntos, ~7.35s pós-boot), causando queda momentânea no rail de 3.3V e reset (`RTC_SW_SYS_RST`). Reparado por substituição física do componente. Ver seção 7 abaixo pra continuação/fechamento desta investigação (a conclusão original desta sessão, mantida abaixo, ainda estava com a causa em aberto no nível de PCB - ficou completa depois).

## Contexto

Continuação de `historico/2026-08-10-reboot-ads1248-segunda-unidade-psram-descartada.md` e `historico/2026-08-10b-reboot-ads1248-isolamento-debug-sw.md`. No fim do dia 10/08, sobravam duas hipóteses concorrentes: estouro de stack/corrupção de RAM (defendida pelo usuário) vs colisão de timing WiFi+USB+ADS1248+heater (hipótese líder da investigação). Plano era testar a primeira (mais barata) antes de migrar o driver do ADS1248 pra `spi_master`.

## 1. Instrumentação de diagnóstico (stack/heap)

Ligado no `sdkconfig`:
- `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y` (era `CANARY`, mais leve)
- `CONFIG_HEAP_POISONING_COMPREHENSIVE=y` (era `LIGHT`)
- `CONFIG_FREERTOS_USE_TRACE_FACILITY=y` (necessário pra `uxTaskGetSystemState`)

Criada task nova `main/mem_monitor.cpp` + `main/include/mem_monitor.h` (registrada em `main/CMakeLists.txt`, chamada em `main.cpp` logo após `print_reset_reason()`): loga a cada 1s, prefixo `[MEM_MON]`, heap livre/mínimo histórico/interno e o `stack high water mark` de todas as tasks do sistema via `uxTaskGetSystemState`.

**Resultado (23 resets capturados numa sessão de ~15min)**: heap sempre saudável (~82-90KB livres, sem tendência de queda), nenhuma task com stack perto de zero (menores foram `ipc0`/`ipc1`, normal do sistema), **zero backtrace/panic/Guru Meditation** em qualquer um dos 23 resets. **Descarta a hipótese de estouro de stack/corrupção de RAM.**

## 2. Migração do driver ADS1248 pra spi_master

`main/light_sensor_ads1248.cpp` reescrito pra usar o periférico de hardware SPI (`driver/spi_master.h`, `SPI2_HOST`) em vez de bit-bang manual em GPIO:
- Modo SPI 1 (CPOL=0, CPHA=1) — confirmado pelo comentário já existente no código sobre bordas de DIN/DOUT do ADS1248.
- CS automático via `spics_io_num`, transações multi-byte agrupadas numa `spi_transaction_t` só (equivalente ao CS ficar baixo durante todo o grupo no bit-bang antigo).
- `RDATA` em duas transações com `SPI_TRANS_CS_KEEP_ACTIVE` + delay de 5us no meio, respeitando o tempo mínimo entre comando e dado (datasheet t6).
- Clock conservador: 500kHz.
- RESET (IO4), START (IO40) e DRDY (IO39) continuam GPIO simples, fora do barramento SPI.

Compilou limpo, flashou, `IDAC0 readback=0x90` (plausível, confirma que o protocolo funciona). **Mas o reset continuou** (14x em ~6min) — **não resolveu**.

Nota histórica: uma tentativa anterior de `spi_master` nesse projeto (`components/cs5534/cs553x.cpp`, comentado) tinha sido abandonada porque o dev anterior tentou compartilhar uma config única de SPI entre CS5534 e ADS1248 (chips com timing diferente). Não é um problema aqui porque os dois drivers são compilados em variantes de firmware separadas (`CONFIG_ADC_CHIP_ADS1248`).

## 3. Descoberta: ADS1248 nem estava ativo (light_sensor_main já comentada)

Ao investigar por que o migrar pra `spi_master` não mudou nada, percebemos que `light_sensor_main()` (a task que leria os 4 canais em loop) **já está comentada** em `main.cpp` no firmware de produção atual — só roda `light_sensor_setup()` uma vez no boot. Ou seja, as duas rodadas de teste acima (bit-bang antigo com mem_monitor, e spi_master novo) já rodaram com o ADS1248 basicamente parado depois do boot, e mesmo assim resetaram.

## 4. Teste decisivo: comentar `light_sensor_setup()` inteira

Comentada também a chamada de `light_sensor_setup()` em `main.cpp`. Com isso, **nenhum pino exclusivo do ADS1248 é tocado pelo firmware**: nem o barramento SPI (SCLK/CS/DIN/DOUT), nem IO4 (RESET), nem IO39 (DRDY, que também é MTCK do JTAG), nem IO40 (START, que também é MTDO do JTAG). Eletricamente equivalente à variante CS5534 nesses pinos.

**Resultado: 58 resets em ~12 minutos** (confirmado por log que `light_sensor_setup()` nunca rodou — zero linha do driver ADS1248), taxa igual ou pior que antes. **Descarta com bastante confiança qualquer envolvimento do ADS1248** — chip, driver, bit-bang vs spi_master, pinos dedicados, pista do JTAG. Não foi necessário desoldar o chip fisicamente pra confirmar isso.

**Atenção**: com `light_sensor_setup()` comentado, qualquer teste de ampola real vai travar (`read_channel_value` chama SPI sem bus inicializado). Reverter antes de qualquer uso normal do equipamento.

## 5. Teste A/B final: mesmo firmware na placa CS5534

Gravado o mesmo `.bin` (com `light_sensor_setup()` ainda comentado) na placa **CS5534** (mesma COM20, board física trocada pelo usuário — confirmado pelo MAC `dc:da:0c:49:07:a8`, que bate com o registro de 07/08).

**Resultado: zero resets em ~10 minutos**, contra 58 resets em ~12min na placa ADS1248 com o firmware idêntico.

Isso repete e confirma um teste A/B que já tinha sido feito em 07/08 (`historico`/memória do bring-up, antes de toda essa investigação): na época, mesma conclusão — CS5534 nunca reiniciou, ADS1248 reiniciava direto no mesmo ponto, com o mesmo binário.

## 6. Teste final: chip ADS1248 fisicamente removido da placa

Pra descartar de vez até a presença elétrica passiva do chip (não só o firmware não tocar nele), usuário dessoldou o ADS1248 fisicamente da placa e testou de novo na mesma COM20, mesmo firmware (com `light_sensor_setup()` ainda comentado).

**Resultado: 41 resets em ~11 minutos, zero backtrace** — mesma cadência de antes (58/12min com o chip presente). **Chip removido não mudou nada.**

## Conclusão

A causa raiz **é física, específica do PCB da placa ADS1248** — não é firmware (mesmo binário rodou estável na CS5534), não é o chip ADS1248 (testado até com ele fisicamente fora da placa, resetou igual), não é WiFi/USB/heater em si (rodam idênticos e estáveis na CS5534). É algo do próprio PCB: componente, regulador local, decoupling ou diferença de layout/ground que existe na revisão ADS1248 e não na CS5534 (ou é diferente entre as duas).

Reforça essa conclusão: o reset já foi reproduzido em **duas unidades físicas diferentes** da placa ADS1248 (ver `historico/2026-08-10-...psram-descartada.md`), o que enfraquece a hipótese de solda fria isolada (defeito de montagem tende a ser aleatório por unidade; duas unidades independentes com o mesmo sintoma aponta pra algo sistemático do design/layout dessa revisão — regulador local, decoupling, EMI, ground).

Também esclarecido nesta sessão (correção do usuário a uma memória antiga): as duas placas usam a mesma versão do PCF8574 (variante "T", endereço 0x20) — a diferença de endereço I2C registrada em 07/08 não é mais válida/relevante.

## Pendente pra próxima sessão

1. **Comparar schematic/BOM das duas revisões de placa** (`Versao ADS1428/*.net` vs equivalente CS5534) procurando componente/trilha que exista numa e não na outra, fora da área do próprio ADS1248 (já descartado). Candidatos: regulador de tensão local, decoupling (valor/posição), plano de terra, outro CI populado só nessa revisão. Osciloscópio na alimentação local (não a entrada geral, já testada e OK) durante a janela conhecida do reset (~7-20s de boot).
2. **Reverter as mudanças de diagnóstico de firmware antes de qualquer uso normal do equipamento**: `main.cpp` com `light_sensor_setup()` comentado, `sdkconfig` com watchpoint/heap poisoning comprehensive (não é bloqueante mantê-los, mas `light_sensor_setup()` comentado quebra teste de ampola real).
3. Já revertido nesta sessão (antes das rodadas de teste acima, a pedido do usuário): pinos do heater/buzzer/DS18B20 de volta pro original (35/36/37) no `sdkconfig`, hack de mascarar sensor ausente removido de `temperature.cpp` (mantido só o fix real do loop sem delay).
4. Decidir se a migração do ADS1248 pra `spi_master` (item 2 acima) vale a pena manter mesmo não sendo a causa do reset — é uma melhoria de robustez legítima (menos CPU ocupada por transferência), independente da causa raiz final.

## Estado do repo ao fim da sessão — NADA COMMITADO

- `sdkconfig`: `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y`, `CONFIG_HEAP_POISONING_COMPREHENSIVE=y`, `CONFIG_FREERTOS_USE_TRACE_FACILITY=y`; pinos heater/buzzer/DS18B20 revertidos pro original (35/36/37).
- `main/mem_monitor.cpp` + `main/include/mem_monitor.h`: novos, registrados em `CMakeLists.txt`.
- `main/main.cpp`: chama `mem_monitor_setup()`; `light_sensor_setup()` **comentada** (diagnóstico, reverter antes de uso normal).
- `main/light_sensor_ads1248.cpp`: reescrito pra `spi_master` (funcional, não é a causa, mas é uma melhoria válida de manter).
- `main/temperature.cpp`: hack de mascarar sensor ausente removido, mantido só o fix real (`vTaskDelay` antes do `continue` quando não acha sensor).
- Placa ADS1248 física (COM20 na maior parte da sessão): trilhas originais 35/36/37 continuam cortadas fisicamente (do dia 10/08), sem jumper novo feito — heater/buzzer/DS18B20 não funcionam fisicamente até isso ser resolvido, apesar do `sdkconfig` já apontar pro pino original de novo.
- Placa CS5534 física: usada só pro teste A/B final (mesma COM20), sem mudança nela.

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`, fullclean+reconfigure+build sempre).
- Flash: `.\flash_Lummina4EdAdj.ps1 -Port COM20`.
- Monitor: `py -m serial.tools.miniterm COM20 115200 --raw`, redirecionado pra arquivo, checado por `grep -c "rst:0x"` depois de um tempo fixo de espera (loop de polling em tempo real se mostrou pouco confiável no Windows — trava esperando `wc -l`/`tail` enquanto o Python do miniterm segura o arquivo aberto).

## 7. Continuação (mesma sessão, tarde) — leitura do esquema elétrico e identificação do componente defeituoso

Retomando o item 1 da lista de pendências acima (comparar schematic/BOM). Só existe a pasta `Versao ADS1428/` no repo (schematic + netlist da placa ADS1248) — sem equivalente da CS5534 pra comparação direta.

### Arquitetura de potência (lida no esquema + confirmada pelo usuário)

Placa alimentada por **P2 (12V)**, passando por um circuito de proteção de entrada (MOSFET + resistor + zener, anti-inversão de polaridade e limitação de picos), que alimenta em paralelo:

- **Aquecedor**: resistência ligada direto em 12V, chaveada por MOSFET, sem regulação.
- **U6 (LM2576TV-5, chaveado)** → catch diode D4 + indutor L1 (100µH) → rail **+5V** → alimenta 4x **U15** (MIC5318YD5-TR, LDO ajustável, um por cavidade) → 4 LEDs UV, ~400mA cada.
- **U2 (LM2576D2T-3.3, chaveado)** → catch diode D3 (MBRD360) + indutor L3 (100µH, DO3316P) → rail **+3V3** → ESP32-S3 (~300mA) + todo o barramento I2C (teclado, 4 painéis de LED de cavidade, sensor de ampola, EEPROM AT24CM02, RTC) + DVDD (digital) do ADS1248. Filtragem de saída perto do U2: C8/C9 (22µF/6V tântalo) + vários 100nF cerâmicos espalhados no mesmo net.

### Observação do usuário sobre os dois reguladores LM2576

- **LM2576 3V3 (U2)**: provoca reset com aumento de consumo — bate com o padrão de reset concentrado no boot (WiFi AP + USB Host subindo juntos), quando a demanda de corrente no rail de 3.3V picoteia.
- **LM2576 5V (U6)**: a tensão de saída cai quando os LEDs UV são acionados (carga de ~1.6A somada dos 4 LEDs) — sintoma de regulação/filtragem insuficiente sob carga, mesmo defeito de categoria do U2, só que no rail de 5V.

**Ambos os reguladores (U2 e U6) foram trocados fisicamente.**

### Testes de isolamento (rodadas 6-12, resumo - detalhe completo na memória do projeto)

1. Fonte de laboratório direto no rail de 3.3V (contornando o U2), chip ADS1248 ausente: **zero resets** em ~14.5min combinados (duas capturas).
2. Chip resoldado de volta + mesma fonte direto no 3.3V: **1 reset em ~9min** + tempestade de ~18.7k erros de I2C (posteriormente atribuída a conector mal encaixado pela solda, não ao chip - confirmado limpo depois).
3. Firmware de produção completo (scaffolding de diagnóstico revertido, driver ADS1248 mantido em `spi_master`) + chip resoldado + fonte direto no 3.3V: **zero resets, zero I2C** em ~5min.
4. **Teste decisivo**: U2 no circuito normal (não mais bypassado), só a ENTRADA de 12V trocada pela fonte de laboratório: **19 resets** em poucos minutos, fortemente concentrados (~16 de 19) no mesmo milissegundo exato do boot (~7.35s), logo após `WIFI SOFTAP: wifi_init_softap finished` + `USB_DAEMON: Installing USB Host Library`. Isolou o defeito no U2/L3/D3/C8-C9, não no 12V externo.
5. **Troca física do U2**: zero resets em ~6min45s (fonte de bancada) e zero resets em ~4min57s (fonte/proteção de entrada originais restauradas).
6. **Validação final de 30+ minutos** (duas sessões de boot, incluindo um power-cycle manual do usuário no meio pra silenciar o alarme de aquecedor): único reset encontrado foi `rst:0x1 (POWERON)` do próprio power-cycle - **zero `RTC_SW_SYS_RST`, zero I2C, zero panic/watchdog** em toda a captura.

### Conclusão final

Causa raiz: **regulador U2 (LM2576D2T-3.3) fisicamente defeituoso** - não sustentava o pico de corrente transitório do boot (WiFi AP + USB Host inicializando juntos). O **U6 (LM2576-5)** também apresentava sintoma relacionado (queda de tensão sob carga dos LEDs UV) e foi trocado junto, por precaução/mesma categoria de defeito. Não era o chip ADS1248, não era firmware, não era o 12V externo nem o circuito de proteção de entrada.

### Achado colateral (não relacionado ao reboot)

Bug real encontrado em `main/ampoule_test.cpp`: `abort_ampoule_test_sensor_fault()` (linha 525) não desliga o LED UV da cavidade quando aborta um teste por falha persistente de leitura do sensor (3 timeouts consecutivos de DRDY). A função `ampoule_test()` (linha 732-741) chama essa função e faz `return` antes de chegar em `finalize_test(ampoule)` (linha 750), deixando o LED aceso pra sempre. Observado nas cavidades 2 e 4 durante essa sessão. Fix sugerido: chamar `led_uv_off(ampoule)` dentro de `abort_ampoule_test_sensor_fault()`. **Não corrigido ainda.**

### Estado do repo ao fim desta continuação

Mesmo do fim da seção anterior, mais: `light_sensor_setup()` descomentada de volta (produção), `mem_monitor.cpp`/`.h` removidos (arquivos deletados, chamada removida de `main.cpp`/`CMakeLists.txt`), `sdkconfig` revertido pros defaults de produção (watchpoint/heap poisoning/trace facility). `light_sensor_ads1248.cpp` mantido em `spi_master` (decisão do usuário). Build gerado e flashado (`Lummina4_MAXXIMED_v2026_08_11.bin`) - nada commitado ainda.
