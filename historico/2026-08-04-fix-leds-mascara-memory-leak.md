# Mudanças desta versão — máscara central de LEDs, memory leaks no webserver, efeito de aquecimento e versão de firmware

**Data:** 04/08/2026
**Branch:** `feature/config-web`
**Motivo:** cliente mandou vídeo (WhatsApp, 03/08) mostrando o padrão de LED esperado durante o aquecimento e reportou o webserver travando com uso prolongado. Investigação dos dois problemas levou a uma limpeza mais ampla de bugs de "LED acende em cavidade desabilitada" que vinham se repetindo em funções diferentes.

## 1. Máscara central de LEDs (`main/led_panel.cpp`)

**Problema:** o bug "LED acende numa cavidade desabilitada (CRC1 ou config web)" já tinha sido corrigido pontualmente antes (`read_ampoules_test_task`, commit `22053ce`), mas reapareceu em **3 lugares diferentes** achados nesta sessão:
- `efeito_giroflex()` — chase de aquecimento tocava P0/P4/P5 nas 4 cavidades sem checar `disabled_status`.
- `check_heater_temperature_task()` (`heater.cpp`) chamava `set_led_function_active()` direto (liga P0 nas 4 cavidades incondicional) em vez do helper já corrigido `ampoule_test_check_cavity_finalize()`.
- `led_panel_main()` também acendia P0 nas 4 cavidades via `digital_write` direto, sem checagem nenhuma.

**Fix estrutural** (sugestão do usuário, em vez de continuar corrigindo caso a caso): máscara aplicada num único ponto de escrita, `change_led_status()` / `set_all()` — `apply_cavity_mask()` bloqueia qualquer tentativa de **acender** (nunca bloqueia apagar) um pino de cavidade (P0-P5) se:
- a cavidade daquele painel estiver desabilitada (`ampoule_is_disabled()`), **ou**
- o modo for **ETO** e o pino for P1/P2/P3 (níveis 1H/2H/3H — ETO só usa o nível 1).

P6/P7 (ícones globais: impressora, wifi, alarme) ficam fora da máscara. Todas as funções que escreviam direto no `.digital_write()` do painel (`set_led_function_active`, `crc1_led_lamp_test_cavity1`, `set_led_function_on_off`, `set_led_function_deactive`, `led_panel_main`, `efeito_giroflex`) foram convertidas para passar por `change_led_status()`/`set_all()`, garantindo que a máscara pega qualquer chamada, existente ou futura.

## 2. Novo efeito de aquecimento (`efeito_giroflex()`)

Reescrito para bater com o vídeo de referência do cliente (WhatsApp Video 2026-08-03 15.02.23, analisado quadro a quadro): por cavidade habilitada, enquanto a temperatura não estabiliza —
"−" (verde) acende sozinho + nível de tempo preenche 20M→1H→2H→3H (acumulando, tipo termômetro) → esvazia de volta 3H→...→20M e o "−" apaga → "+" (vermelho) acende sozinho → "+" e "−" juntos → apaga tudo → repete.

Substitui o efeito antigo (chase rápido entre 2 LEDs). Passo de 300ms entre estágios — não deu pra confirmar o timing exato pelo vídeo (câmera de celular, taxa de amostragem baixa), é um valor razoável, fácil de ajustar.

## 3. `boot_lamp_test()` — regressão restaurada

Comparando com o firmware original (`D:\Github\ECK\Maxximed\Lummina4Ed`, pré-CRC1, commit único "Versão inicial do projeto Lumina4"): `led_panel_setup()` tinha um self-test no boot (`set_all(0)` → espera 1s → `clear_all()` → espera 1s, todos os 8 pinos, todos os 4 painéis) que **sumiu em algum refactor** deste repo, sem ninguém remover de propósito. Reimplementado como `boot_lamp_test()`, chamado em `main.cpp` logo depois de `ampoule_apply_cavity_enabled_config()` (pra já ter a config de cavidades carregada) — diferente do original, agora respeita a máscara (só pisca cavidade habilitada).

`sensor_error.cpp` tem um comentário-spec de um checkup de boot parecido (nunca implementado, arquivo só com comentário) — não mexido, é um escopo maior (UV, fotodiodo) que não foi pedido nesta sessão.

## 4. Memory leaks no webserver (`main/app_httpd.cpp`)

Investigando por que o AP trava com uso prolongado (além do problema de Wi-Fi já documentado): **5 dos 8 handlers** que usam o padrão `malloc(SCRATCH_BUFSIZE)` (10KB) pra receber corpo de POST **nunca chamavam `free()`** — vazavam 10KB por request, em todo caminho de saída, sem exceção:

- `translate_json_post_handler` (`/api/v1/translate`)
- `api_language_post_handler` (`/api/v1/language`)
- `device_settings_post_handler` (`/api/v1/device/settings`)
- `api_settings_serialnumber_post_handler` (`/api/v1/restrict/serialnumber`)
- `restrict_device_settings_post_handler` (`/api/v1/restrict/settings`)

Corrigidos seguindo o mesmo padrão dos 3 handlers que já estavam certos (`free(buf)` nos dois early-returns + logo após o `cJSON_Parse`). `translate_json_post_handler` também tinha um `cJSON_Delete(root)` faltando no branch 404.

**Validado por stress test via `fetch()` no console do Chrome**: 2300 requisições em `/api/v1/translate` e 300 em `/api/v1/language`, 100% OK, sem degradação de latência — antes do fix isso teria vazado ~26MB de heap.

**Achado à parte durante o stress test** (não é o memory leak, é outra coisa): 1000 requisições seguidas em `/api/v1/language` (que grava na NVS a cada chamada) fizeram o equipamento resetar sozinho (`POWERON_RESET` — motivo ambíguo, não é watchdog/abort claro; pode ser brownout de energia sob carga de Wi-Fi alta, não investigado a fundo). Fora do escopo do fix de hoje, mas vale registrar caso apareça de novo.

## 5. Versão de firmware

`CONFIG_FIRMWARE_VERSION` (exibida na tela de configurações) estava travada em `"2025.03.11"` (`sdkconfig`) — `sdkconfig.defaults` tinha um valor diferente ainda (`"2023.09.05"`). Os dois atualizados para `"2026.08.04"`. O nome do `.bin` (`Lummina4_MAXXIMED_v<data>.bin`) já era gerado automaticamente com timestamp de build (`CMakeLists.txt`) — só a string exibida na tela que estava hardcoded/desatualizada.

## 6. Bug de JS na tela de config avançada (`components/httpd_app/www/pages/advanced_config.html`)

Os handlers de `change` dos checkboxes `mode_normal`/`mode_eto`/`mode_crc1` cuidam da exclusão mútua (desmarcar os outros dois) mas **nunca chamavam `updateCrc1CavityLock()`** — a função que reabilita os checkboxes de cavidade ao sair do CRC1. Ela só rodava uma vez, no carregamento da página. Resultado: depois de entrar em CRC1 (que trava as cavidades 2-4), trocar pra Normal/ETO **na mesma sessão da página, sem recarregar**, deixava as cavidades 2-4 travadas pra sempre (visualmente cinza, `disabled=true` mesmo com o modo já trocado). Corrigido chamando `recalculateMinimums()` (que já chama `updateCrc1CavityLock()`) nos três handlers.

Fix e `.gz` regenerado e gravado nesta mesma sessão (ver item 7).

## 7. Bug de fuso horário (-3h) na tela de Histórico (`components/httpd_app/www/pages/history.html`)

Achado numa conversa lateral sobre "o que acontece se a impressora estiver desligada" — o usuário reparou que um ticket reimpresso a partir do histórico mostrava `06:53:55` quando o horário certo (visto ao vivo na tela de Incubação, e confirmado como o RTC "está certo") era `09:53:55`.

Causa: `history.html` formatava com `moment.unix(rec.tsInicio).format("DD/MM/YYYY HH:mm:ss")` — sem `.utc()`, o `moment` aplica o **fuso horário do navegador** por cima do valor. O `ts_inicio` salvo pelo equipamento é a hora local crua (do RTC) tratada como se fosse epoch UTC — o firmware não tem fuso configurado em lugar nenhum (`mktime`/`localtime_r` sem `setenv("TZ",...)`/`tzset()` em todo o projeto, de propósito, então são simétricos e sem offset). Confirmado numericamente: o epoch `1785837235` convertido como UTC puro dá exatamente `09:53:55` — o navegador (rodando em fuso Brasília, UTC-3) estava subtraindo mais 3h de um valor que nunca foi UTC de verdade.

Fix: `.utc()` depois do `moment.unix()`, pra impedir o `moment` de aplicar qualquer fuso.

**Não verificado**: se o ticket de **papel** físico (caminho C++, `format_ts()` em `main/printer.cpp`) também imprimia a hora errada. O raciocínio é que não deveria — mesma ausência de fuso no firmware, mas simétrica (`mktime` grava, `localtime_r` lê, sem TZ nos dois lados) — mas não foi testado imprimindo de verdade nesta sessão.

## Testado no equipamento (COM20 + Wi-Fi AP `MAX-4907A8`, 192.168.10.10)

- Build limpo em todas as etapas (Docker `edsoncan/espressif-idf:release-v5.1`).
- CRC1: confirmado fisicamente que só a cavidade 1 pisca agora (antes vazava pras cavidades 2-4 mesmo com elas travadas na tela).
- Novo efeito de aquecimento gravado (aguardando confirmação visual final contra o vídeo de referência).
- `boot_lamp_test()` gravado, comportamento no boot ainda não confirmado visualmente contra o vídeo em condição real de boot frio→quente completo.
- Memory leak fix validado via stress test (2600 requisições, 0 falhas, sem degradação).
- Bug de JS das cavidades (item 6) e bug de fuso do histórico (item 7): corrigidos, `.gz` regenerado e **gravado** — sem confirmação visual do usuário ainda no equipamento.

## Pendente pra próxima sessão

- Confirmar visualmente o novo `efeito_giroflex()` e o `boot_lamp_test()` contra o vídeo de referência do cliente numa bateria de boot frio→quente completa.
- Confirmar o sweep LED12→13→14 do CRC1 (`crc1_led_lamp_test_cavity1`) — precisa deixar a resistência ligada até passar de `heater_release_temp_c` (35°C default), não aconteceu nesta sessão.
- Confirmar na tela web que as cavidades destravam corretamente ao trocar de modo sem reload, e que o histórico mostra a hora certa agora.
- Testar se o ticket de papel físico tinha (e agora não tem) o bug de -3h (item 7).
- Investigar o reset por `POWERON_RESET` sob stress de `/api/v1/language` (não é o memory leak, causa ainda não identificada — suspeita de brownout).
- Equipamento ficou em modo Normal, 4 cavidades habilitadas (estado do fim da sessão anterior, 03/08) — não restaurado a um estado "de fábrica" de propósito, fica assim pro cliente ver.
