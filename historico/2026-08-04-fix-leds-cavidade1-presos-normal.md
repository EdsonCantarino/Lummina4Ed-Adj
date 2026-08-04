# Fix — LEDs 20min/1h/2h/3h da cavidade 1 travados acesos no idle (modo Normal)

**Data:** 04/08/2026
**Branch:** `feature/config-web`
**Motivo:** cliente reportou (WhatsApp, vídeo + foto da própria configuração) que ao habilitar a máquina para uso, os LEDs de tempo (20min, 1h, 2h, 3h) da cavidade 1 ficam acesos continuamente, diferente das cavidades 2-4 que ficam apagadas nesse estado. Confirmado pelo cliente que o bug só acontece em modo **Normal**.

Configuração do cliente no momento do relato: 4 cavidades ativas (01-04), modo Normal, tempo de LED aceso antes da leitura 0,5s, tempo de looping 4s, setpoint 60°C (mín. 53°C / máx. 66°C).

## Causa raiz (`main/led_panel.cpp`)

Regressão introduzida pelo próprio fix de hoje mais cedo (`4c2db37`, ver `historico/2026-08-04-fix-leds-mascara-memory-leak.md`, item 2) — a reescrita de `efeito_giroflex()` passou a animar P0-P3 (LEDs 20min/1h/2h/3h) além de P4/P5, acumulando por cavidade num ciclo de ~2,9s.

`start_stop_led_effect_test(false)` (`led_panel.cpp:573-590`, não tocada nesse commit) suspende a task da animação (`vTaskSuspend`) quando a máquina é habilitada, mas só limpava P4/P5 — resquício de quando a animação antiga só usava esses dois pinos. Como `vTaskSuspend` congela a task exatamente onde estava, se a suspensão cair no meio do ciclo de acúmulo de P0-P3, esses pinos ficam fisicamente presos em ON. O laço da animação (`for (int i=0; i<4; i++)`) sempre começa pela cavidade 1, e como a task roda desde o boot sem nunca ter sido suspensa antes, a primeira estabilização de temperatura tende a interrompê-la ainda na 1ª cavidade — daí o padrão sempre na cavidade 1.

**Por que só em modo Normal:**
- ETO: a máscara central (`apply_cavity_mask`, `led_panel.cpp:56-58`) já bloqueia incondicionalmente P1-P3 em qualquer cavidade — o vazamento nunca acende fisicamente.
- CRC1: `crc1_led_lamp_test_cavity1()` roda logo depois de `start_stop_led_effect_test(false)` (`heater.cpp:268-276`) e varre/apaga P0-P3 explicitamente, mascarando o mesmo bug por acidente.
- Normal: nenhuma das duas proteções existe — o bug fica visível.

Não é diferença de hardware/GPIO entre cavidades: os 4 painéis (`led_panel_1..4`) são inicializados de forma idêntica.

## Fix

`start_stop_led_effect_test(false)` agora limpa P0-P5 (não só P4/P5) nas 4 cavidades ao suspender a animação. Verificado que não conflita com o fluxo seguinte: `ampoule_test_check_cavity_finalize()` → `set_led_function_on_off()` já reescreve P0 com o estado correto (habilitada/desabilitada) por cavidade logo em seguida.

## 2. Limite de reimpressão automática de pendentes (`main/printer.cpp`)

**Motivo:** quem usa o equipamento sem impressora conectada acumula histórico de tickets não impressos; ao conectar a impressora depois, `print_pending_unprinted_history()` reimprimia **todos** de uma vez — potencialmente uma pilha grande de papel de uma hora pra outra.

**Fix:** nova constante `PENDING_REPRINT_MAX` (4). Se houver mais de 4 pendentes, imprime só os 4 mais recentes de verdade e marca os mais antigos como impressos **sem imprimir** (`ampoule_history_mark_printed`, sem passar por `print_ampoule_test_history`) — evita tanto a pilha de papel quanto deixar pendentes "escondidos" indefinidamente atrás de um registro já marcado como impresso (a varredura em `print_pending_unprinted_history()` sempre começa do índice mais recente e para no primeiro já impresso, então um pendente antigo demais nunca seria alcançado de novo).

## 3. Tempo fixo do modo ETO: 5min → 20min (`main/ampoule_test.cpp`, `advanced_config.h`)

**Motivo:** cliente pediu pra mudar o tempo de teste do ETO de 5 para 20 minutos, sem alterar Normal/CRC1 (que devem continuar iniciando em 5 min, comportamento adicionado no commit de ontem `d42b6cf`).

**Causa raiz do "5 min" no ETO:** o botão físico de tempo fica desabilitado em ETO (`keyboard.cpp`), então a cavidade sempre usa o valor default/"nível 0" (`DEFAULT_TIME_TEST`, `ampoule_test.cpp:29`). Até ontem esse default era 20 min — coerente com o comentário em `advanced_config.h` ("ETO: tempo fixo de 20min"). O commit `d42b6cf` mudou a sequência de botões de Normal/CRC1 pra incluir um novo nível "5 min" e trocou esse mesmo default de 20 para 5 min como efeito colateral, arrastando o tempo fixo do ETO junto sem ninguém perceber.

**Fix:** em vez de reverter `DEFAULT_TIME_TEST` (o que tiraria o "5 min" novo de Normal/CRC1), força `ampoules[index].time_test = 20 * 60` só quando `g_advanced_config.operation_mode == OPERATION_MODE_ETO`, no início de cada teste (`ampoule_test()`, bloco `test == 1`) — cobre também o caso de trocar de modo pela tela web em tempo real sem reboot, que deixaria um valor herdado de Normal/CRC1. Comentário desatualizado em `advanced_config.h` corrigido de "20min/1h/2h/3h" pra "5min/20min/1h/3h".

## 4. Ticket: "Tempo de leitura" sem segundos e em formato `HHhMMmin` (`main/printer.cpp`)

Duas mudanças no campo "TEMPO DE LEITURA" do ticket impresso:
- Passou a usar `strip_seconds()` (já usado pra hora de início) e depois um formato dedicado `format_duration_hm()`, convertendo "HH:MM:SS" pra "HHHMMMin" (ex.: `00H00Min`, `01H23Min`) — pedido do cliente, formato mais legível que "00:00" pra uma duração.

## Testado no equipamento (COM20, ESP32-S3 `dc:da:0c:49:07:a8`, 192.168.10.10)

- Build limpo via Docker `espressif/idf:release-v5.1` em cada etapa (só os warnings pré-existentes de `LED_ON`/`LED_OFF` redefinidos em `led_panel_pin_mapping.h`, não relacionados).
- Gravado com sucesso (`esptool`, reset via RTS) já com os 4 fixes juntos.
- Configuração do cliente replicada na tela web (`/admin/advanced_config`): modo Normal, 4 cavidades, 60/53/66°C, 0,5s/4s, checagem antecipada 3min — salva com sucesso.
- Log serial (COM20) após reset, com a config do cliente aplicada: `Temperatura estabilizada: Sim` / `Funcoes Habilitadas: Sim` / `Temperatura no Range > 53.0 e < 66.0: Sim` já nos primeiros segundos de boot (equipamento já estava quente) — confirma que o caminho de código corrigido (item 1) rodou, mas **não confirma visualmente** o estado físico dos LEDs.
- Cliente rodando um teste de 5 min no equipamento físico no momento em que este arquivo foi salvo — resultado ainda não registrado aqui.
- **Nenhum dos 4 itens foi confirmado visualmente ainda** (LEDs cavidade 1 apagados no idle, reimpressão limitada a 4, ETO em 20min, formato novo do ticket).

## Pendente pra próxima sessão

- Confirmar visualmente no equipamento os 4 itens desta sessão.
- Reconfirmar junto os itens já pendentes do fix anterior de hoje (`efeito_giroflex()` e `boot_lamp_test()` contra o vídeo de referência, sweep do CRC1, destravamento de cavidades ao trocar de modo sem reload, bug de fuso do histórico, reset por `POWERON_RESET` sob stress) — ver `historico/2026-08-04-fix-leds-mascara-memory-leak.md`.
