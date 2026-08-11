# Investigação — reboot ADS1248: isolamento por firmware mínimo (Debug_SW) e reprodução da causa

**Data:** 10/08/2026 (continuação da sessão da manhã)
**Branch:** `feature/config-web`
**Status:** causa raiz ainda **não isolada por completo**, mas **reproduzida** de forma controlada pela primeira vez, e uma linha de investigação clara foi definida pra amanhã (migrar ADS1248 pra SPI de hardware). Ver também `historico/2026-08-10-reboot-ads1248-segunda-unidade-psram-descartada.md` (parte 1 do dia).

## Recapitulando o que já vinha da parte 1

Teoria da PSRAM (IO35/36/37) descartada por teste físico decisivo (trilha cortada). Sobrava investigar reset silencioso (`RTC_SW_SYS_RST`, sem log algum antes) sem causa de software conhecida identificada.

## O que foi feito depois (tarde/noite de 10/08)

### 1. Pino EN medido com ponta lógica
Usuário mediu fisicamente o pino 3 (EN-PROG) e o pino 27 (IO0/IO-PROG) durante o reset: **ambos permanecem em nível alto o tempo todo**. Confirma que o reset é 100% interno ao chip (não é reset externo por hardware/botão/supervisor) — bate com o motivo já reportado via serial.

### 2. Teoria do brownout interno
`CONFIG_ESP_SYSTEM_BROWNOUT_INTR=y` faz o brownout do ESP32-S3 passar pela mesma rotina de reset genérica que `esp_restart()` usa (por isso sempre aparece como "Software reset digital core", mesmo se fosse brownout) — explicaria o silêncio total. Nível de detecção estava no mais sensível (`CONFIG_ESP_BROWNOUT_DET_LVL=7`).

**Teste feito**: descomentada a linha `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)` já existente (comentada) no `main.cpp`, pra desligar o brownout via registrador bruto. **Resultado: piorou muito** — reset praticamente contínuo (~0-1s entre cada um). Interpretação: escrever `0` no registrador inteiro provavelmente zera outros campos de configuração junto (não só o bit de enable), deixando num estado inválido — teste inconclusivo/malfeito, não repetido de forma limpa. **Revertido** (linha comentada de volta, estado original restaurado e regravado).

### 3. Capacitores físicos
Usuário testou: eletrolítico 220µF, depois +mais um (total ~300µF) perto do ESP32, e depois um cerâmico/poliéster de 470nF a menos de 5mm do chip (decoupling de alta frequência). **Nenhum eliminou o reset** no firmware completo de produção. Runs individuais chegaram a ~150-340s antes de resetar de novo (mais longos que o padrão inicial, mas não eliminado).

### 4. Docker Desktop deu erro (HTTP 503)
Resolvido reiniciando o Docker Desktop pela bandeja do Windows + aceitando update/extensões que ele pediu.

### 5. Firmware mínimo de diagnóstico: `Debug_SW`
Criado um projeto ESP-IDF **separado e descartável** em `D:\Github\ECK\Maxximed\Debug_SW` (fora do repo do Lummina4Ed Adj), pra isolar subsistemas de produção um por um na mesma placa física (COM20) e ver qual combinação reproduz o reset silencioso. Componentes copiados/adaptados do projeto principal: `usb_printer` (USB Host, igual produção), `ads1248` (registradores) + `light_sensor_ads1248` (driver bit-bang, adaptado pra não depender do header compartilhado com CS5534), `wifi_ap` (adaptado, sem depender de `led_panel.h`). Além de I2C direto (driver nativo) pros LEDs do painel (feedback visual) e um servidor HTTP mínimo com `esp_http_server`.

**Bugs achados e corrigidos nesse processo (não é a causa raiz, mas reais):**
- `usb_daemon_setup()` tem uma sincronização de semáforo binário que matematicamente não fecha (`usb_daemon_setup()` espera 2 sinais, só existe 1 `xSemaphoreGive()` no código, `class_driver_task` consome esse sinal pra si). Em produção isso não trava o sistema porque `usb_printer_setup()` é a última coisa chamada em `setup()` — todo o resto já roda em tasks independentes antes disso. No firmware de teste, resolvido colocando o blink de LED em task própria, iniciada antes do USB Host.
- Stack overflow real: `blink_task` e `heater_sim_task` com só 2048 bytes de stack estouravam sob certas condições (capturado com `vApplicationStackOverflowHook` + backtrace real, algo que a produção nunca mostra). Corrigido aumentando pra 4096 bytes. Isso confirmou que o ESP-IDF/panic handler consegue sim imprimir backtrace quando é uma crash "normal" — reforça que o reset da produção não passa por nenhum desses caminhos conhecidos.

### 6. Descoberta: IO39/40/41/42 são os pinos de JTAG do ESP32-S3
`IO39=MTCK, IO40=MTDO, IO41=MTDI, IO42=MTMS`. O driver do ADS1248 já usa IO39 (DRDY) e IO40 (START) desde sempre, e a migração de pinos feita na parte 1 do dia (heater→IO42, DS18B20→IO41) também caiu em pinos de JTAG por descuido — mesma categoria de erro que a PSRAM, mas **mais fraca**: JTAG é só função alternativa do mux de GPIO (controlada por strapping do GPIO3 + eFuse), não uma ligação física fixa dentro do chip como a PSRAM. Por padrão, sem depurador JTAG conectado, deve comportar-se como GPIO normal. **Não investigado a fundo ainda** (ficou em segundo plano depois que WiFi reproduziu o problema) — vale reconsiderar se a causa não fechar por outro lado.

### 7. Isolamento progressivo — resultados

| Combinação testada | Resultado |
|---|---|
| I2C + USB Host | Estável, 758s |
| I2C + USB Host + ADS1248 (sem heater) | Estável, 482s |
| I2C + USB Host + ADS1248 + heater alternando a cada 3s (padrão errado, ver abaixo) | Estável ~410s |
| I2C + USB Host + ADS1248 + heater **ligado contínuo** (padrão real da fase fria) | Estável, rodou várias centenas de segundos sem resetar isoladamente |
| **I2C + USB Host + ADS1248 + heater contínuo + WiFi AP + HTTP** (tudo) | **REPRODUZIU o reset exato da produção** (`RTC_SW_SYS_RST`, zero log antes, silencioso) — primeira reprodução isolada da investigação |
| I2C + WiFi AP + HTTP (sem USB/ADS1248/heater) | Estável, 760s |
| I2C + WiFi AP + HTTP + USB Host (sem ADS1248/heater) | Estável, 1255s+, heap livre perfeitamente constante (sem vazamento) |

**Correção de modelo importante feita no meio do caminho** (observação do usuário): o heater real (`heater.cpp`) não alterna o GPIO periodicamente — `heater_start()` liga o pino UMA VEZ e ele fica em nível alto **contínuo** enquanto a temperatura estiver abaixo da banda de histerese (fase fria/subindo). Só alterna quando já perto do setpoint. Bate com a observação física: o problema só acontece **antes** de atingir a temperatura de operação, e (já sabido desde 07/08) acontece mesmo com a resistência física desconectada — não é sobre corrente de carga real, é sobre o estado/firmware durante a fase de aquecimento.

### 8. Hipótese líder atual

Não é nenhum subsistema isolado, é a **carga combinada** (WiFi + USB Host + ADS1248 + heater) rodando junto que bate num limite. Heap descartado como causa (ficou perfeitamente estável no teste WiFi+USB). Timing dos resets é **determinístico/repetível** (mesmo milissegundo em dezenas de boots), não parece ruído elétrico aleatório — aponta pra **colisão de timing de CPU/interrupção**, não corrupção de memória nem EMI.

Suspeita principal: o bit-bang do ADS1248 usa espera ocupada (`ads1248_delay()`, loop `for` sem ceder CPU) por várias iterações seguidas dentro de cada transferência SPI. Isso pode ocasionalmente atrasar demais o atendimento de interrupção que o WiFi exige com prioridade alta/tempo real, e o USB Host competindo por interrupção/DMA ao mesmo tempo pode ser o que faz essa colisão ultrapassar algum limite (ex: interrupt watchdog, ou algo mais interno que não passa pelo panic handler normal).

**Ainda não confirmado** — faltou testar WiFi+USB+ADS1248 (sem heater) e WiFi+USB+heater (sem ADS1248) separados pra saber se precisa dos dois juntos ou só um.

## Decisão do usuário pra retomar amanhã

**Migrar a interface do ADS1248 do bit-bang atual pra usar o periférico de SPI de hardware interno do ESP32-S3** (`spi_master`), em vez do bit-bang manual em GPIO. Motivação direta: se a hipótese de colisão de timing/interrupção do bit-bang estiver certa, usar o periférico de hardware (transferência via FIFO/DMA, sem loop de espera ocupada bloqueando a CPU) deve eliminar a fonte do problema.

**Atenção**: isso contraria uma decisão anterior já registrada — `spi_master` foi tentado e abandonado antes nesse projeto (ver `components/cs5534/cs553x.cpp`, comentado) e o usuário tinha pedido explicitamente pra manter bit-bang. **Motivo do abandono anterior, esclarecido pelo usuário nesta sessão**: o Luiz (dev anterior) não conseguiu compatibilizar o modo SPI (polaridade/fase de clock) entre CS5534 e ADS1248 usando `spi_master` — os dois chips têm requisitos de timing SPI diferentes e ele não achou uma configuração do periférico de hardware que funcionasse bem pros dois ao mesmo tempo. Isso é resolvível: os dois drivers já são compilados separadamente hoje (chave Kconfig `CONFIG_ADC_CHIP_ADS1248`), então a migração pode configurar o `spi_master` com o modo SPI específico de cada chip dentro do seu próprio driver, sem precisar de uma config única compatível com os dois — o problema do Luiz era provavelmente tentar usar uma instância/config compartilhada.

## Hipótese alternativa levantada pelo usuário: estouro de stack/RAM

Usuário não está convencido pela teoria de colisão de timing/interrupção com o WiFi — acha mais provável **estouro de stack pointer ou corrupção de RAM** em algum momento. Bate com o próprio bug real de stack overflow encontrado no firmware de teste hoje (seção 5 acima).

Conferido o `sdkconfig` de produção: hoje usa as versões **mais leves** de detecção —
`CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY` (checa só em troca de contexto) e `CONFIG_HEAP_POISONING_LIGHT` (checa só nas bordas do bloco, no alloc/free). Existem versões mais rigorosas disponíveis e hoje desligadas:
- `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK` — watchpoint de hardware, pega o estouro de pilha no instante exato que acontece.
- `CONFIG_HEAP_POISONING_COMPREHENSIVE` — checagem mais completa de corrupção de heap (mais cara em CPU/RAM, mas mais confiável).

**Testes concretos pra amanhã** (mais baratos que migrar pro spi_master, fazer antes):
1. Ligar `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK` + `CONFIG_HEAP_POISONING_COMPREHENSIVE` no firmware de produção completo (não o Debug_SW) — se for corrupção real, deve aparecer um backtrace real dessa vez, ao invés do reset silencioso.
2. Adicionar log periódico de `esp_get_free_heap_size()` / `esp_get_minimum_free_heap_size()` / `uxTaskGetStackHighWaterMark()` das tasks mais suspeitas (heater, light_sensor, usb) na produção, pra ver se a memória cai gradualmente ou alguma pilha específica fica perto do limite antes do reset.

## Estado dos repositórios ao fim da sessão

**`Lummina4Ed Adj` (repo principal) — NADA COMMITADO:**
- `sdkconfig`: `CONFIG_HEATER_GPIO=42`, `CONFIG_BUZZER_GPIO=38`, `CONFIG_DS18X20_ONEWIRE_GPIO=41` (divergindo do padrão 35/36/37 de produção).
- `main/temperature.cpp`: hack temporário mascarando ausência do sensor DS18B20 (força 35.0°C).
- `main/main.cpp`: sem mudança líquida final (linha do brownout foi editada e revertida de volta ao estado original comentado).
- Placa física ADS1248 (COM20): as 3 trilhas originais (IO35/36/37) continuam **fisicamente cortadas**. Sem jumper feito ainda pros pinos novos (42/38/41) — heater, buzzer e leitura de calibração DS18B20 não funcionam fisicamente até isso ser feito.
- Osciloscópio/multímetro: nada de anormal encontrado na alimentação geral nem no pino EN.
- Capacitores extras (220+ eletrolíticos + 470nF cerâmico) ficaram fisicamente soldados na placa.

**`Debug_SW` (projeto novo, descartável, fora do repo principal, em `D:\Github\ECK\Maxximed\Debug_SW`)**:
- Não commitado em git nenhum (nem tem repo git próprio). Usuário disse que vai salvar tudo em outro HD, não precisa commitar.
- Mantém o firmware que reproduziu o reset (WiFi+HTTP+USB+ADS1248+heater contínuo) — útil reaproveitar amanhã como base pro teste do `spi_master`.
- Componentes: `components/usb_printer`, `components/ads1248`, `components/light_sensor_ads1248`, `components/wifi_ap`.
- Scripts: `compila_debug.ps1` (mesmo padrão docker do projeto principal, sem o parâmetro `-DLOGO`).

## Pendente pra próxima sessão

1. **Migrar o ADS1248 pra `spi_master`** (decisão do usuário) — investigar primeiro o motivo do abandono anterior do `spi_master` nesse projeto antes de reimplementar.
2. Terminar de isolar: WiFi+USB+ADS1248 (sem heater) vs WiFi+USB+heater (sem ADS1248), pra saber se precisa dos dois juntos ou só um — pode não ser mais necessário se a migração pro `spi_master` já resolver de vez.
3. Reconsiderar a pista do JTAG (IO39/40/41/42) se a migração pro `spi_master` não resolver sozinha.
4. Fazer os jumpers físicos definitivos (heater→IO42, buzzer→IO38, DS18B20→IO41) e religar o sensor DS18B20, ou reverter pros pinos originais — decidir depois que a causa raiz for confirmada.
5. Testar a mesma combinação (WiFi+USB+ADS1248 bit-bang) na placa CS5534 pra ver se ela também falha sob a mesma carga — ainda não sabemos por que só a ADS1248 falha (código de WiFi/USB/heater é idêntico nas duas variantes de firmware).

## Ambiente

- Build produção: `.\compila_Lummina4EdAdj.ps1`. Build Debug_SW: `.\compila_debug.ps1` (mesmo Docker `espressif/idf:release-v5.1`).
- Flash: `py -m esptool` direto (com os parâmetros do `flash_Lummina4EdAdj.ps1`) pros dois projetos.
- Monitor: `py -m serial.tools.miniterm COM20 115200 --raw` em background, filtrado por `rst:0x...,boot:` real (não pelo texto genérico de histórico de reset da NVS).
