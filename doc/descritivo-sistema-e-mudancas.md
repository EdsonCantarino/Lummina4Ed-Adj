# Lummina 4 — Descritivo do Sistema e Registro de Mudanças

**Data de referência:** 23/07/2026 (última atualização: 23/07/2026, tarde — item 6/temperatura)
**Status:** Itens 1 a 5 implementados (commit `85fab6e`), testados ao vivo no equipamento (página, salvar, restaurar, bloqueio durante teste, faixas e mínimos cruzados — ver seção 9 do `historico/`) e com uma correção aplicada (bloqueio de `ampoule_any()` faltando em `restore_defaults`, ver seção 9 do `historico/`). Item 6 (temperatura) implementado nesta mesma sessão, aguardando gravação no equipamento e teste ao vivo (ver seção 7 abaixo). Branch `feature/config-web`, nada commitado ainda além do `85fab6e`.
**Documentos relacionados:**
- `Proposta_Configuracoes_Web.txt` (raiz) — versão para o cliente, linguagem simples.
- `historico/2026-07-23-configuracoes-web.md` — análise técnica detalhada por item (arquivos, linhas, riscos).
- Este documento — visão geral do sistema + registro de decisões de projeto e correções, para consulta durante a implementação.

---

## 1. Como o sistema funciona hoje

Firmware ESP32 (ESP-IDF 5.1, compilado via Docker — `espressif/idf:release-v5.1`) para a Lummina 4, incubadora biológica com 4 cavidades para teste de ampolas.

### 1.1 Fluxo de um teste de ampola

1. O sensor de presença física (PCF8574, I2C) detecta a inserção da ampola em uma cavidade (`main/ampoule_sensor.cpp`, `read_ampoules()`).
2. Com a temperatura estabilizada (>= 33-35°C, `main/heater.cpp`), o teste começa (`main/ampoule_test.cpp`, `ampoules_test_timer_task`).
3. A cada ciclo (hoje fixo em 30 segundos: `TASK_TIME` 28000ms + `TASK_TIME_DELAY` 2000ms), para cada cavidade com ampola presente:
   - Liga o LED UV da cavidade (`led_uv_on`).
   - Aguarda um tempo fixo (hoje 2000ms) para o LED atingir leitura estável (`prepare_test`).
   - Desliga o aquecedor durante a leitura.
   - Lê o valor do sensor óptico via conversor A/D de 24 bits CS5532 (`main/light_sensor.cpp`, `read_channel_value` — leitura com overhead próprio de ~150-200ms).
   - Desliga o LED, religa o aquecedor (`finalize_test`).
   - Guarda a amostra lida (`AmpouleSensor::add_sensor_value`, buffer hoje limitado a 10 valores).
4. O teste dura, hoje, sempre 20 minutos fixos (`DEFAULT_TIME_TEST`, `ampoule_set_time_test` ignora qualquer parâmetro recebido).
5. A partir de 7 minutos (`420` segundos, fixo), o sistema passa a checar se o resultado já pode ser considerado positivo antecipadamente — comparando a soma das 5 primeiras amostras com a soma das 5 últimas (`AmpouleSensor::get_test_result`, faixa hoje fixa 5+5). Se positivo, encerra e imprime antes da hora. Se continuar negativo, segue até os 20 minutos e imprime lá.
6. O resultado impresso (`main/printer.cpp`) usa uma impressora térmica USB.

### 1.2 Persistência de configuração hoje

- `main/nvs_utils.cpp` + `components/nvs_helpers`: cada configuração (percentual de positivação, fator de calibração, número de série, etc.) é gravada isoladamente, uma chamada `nvs_set_*` + `nvs_commit` por campo — sem redundância, sem CRC de aplicação (só o que a NVS do ESP-IDF já garante internamente por chave).
- Já houve um incidente real de configuração corrompida por queda de energia durante uma gravação (motivador principal da mudança nº 6 abaixo).

### 1.3 Interface web hoje

Servidor HTTP embarcado (`main/app_httpd.cpp`) servindo páginas estáticas pré-comprimidas (gzip), embutidas no binário via `components/httpd_app/CMakeLists.txt` (`EMBED_FILES`). Páginas existentes: `/` (settings), `/calibration`, `/reset`, `/admin` (monitoramento de ampolas), `/admin/restrict` (percentual de positivação), `/admin/serialnumber`.

**Proteção por senha, hoje, não é uniforme** (achado importante durante o planejamento):
- `/admin/restrict` (GET e POST): protegida por HTTP Basic Auth (`check_basic_auth`, senha "mestre", `getPasswordMaster()`).
- `/calibration` (GET e POST `/api/v1/device/calibration`): **sem nenhuma proteção de senha**.
- `/reset` (GET sem senha; POST `/api/v1/reset` com uma segunda senha diferente, `getPassword()`, digitada em campo de texto na própria página — mecanismo separado do Basic Auth).

**Bloqueio durante operação, hoje, também não é uniforme:**
- Calibração e Reset bloqueiam salvar se `ampoule_any()` (alguma ampola presente/em teste) for verdadeiro.
- Restrict (percentual de positivação) **não bloqueia** — pode ser alterado com teste em andamento.

---

## 2. As 5 mudanças a implementar

Resumo (detalhamento completo em `historico/2026-07-23-configuracoes-web.md`):

1. **Tempo de captura (LED aceso antes da leitura):** de fixo (2s) para configurável, 0,5 a 7 segundos.
2. **Tempo de reinício do looping:** de fixo (30s reais, não 20s como relatado inicialmente) para configurável, 2 a 50 segundos, com mínimo dinâmico dependente dos itens 1 e 4.
3. **Amostras iniciais/finais para cálculo do resultado:** de fixo (5+5) para configurável, 3 a 10 cada, independentemente.
4. **Cavidades ativas:** nova funcionalidade (não existia) — 4 checkboxes, 1 a 4 cavidades ativas simultaneamente (bloqueado deixar 0 ativas).
5. **Tempo mínimo para checagem antecipada de resultado positivo:** de fixo (7 min) para configurável, 3 a 15 minutos.

Todas as 5 configurações ficarão numa **página nova** (`/admin/advanced_config`), dentro da Área Restrita, e não dentro da `restrict.html` existente (decisão tomada por causa da poluição visual que teríamos numa tela de celular padrão se colocássemos tudo junto com o percentual de positivação).

Um sexto item (temperatura) foi pedido pelo cliente depois da implementação inicial dos 5 primeiros — ver seção 7.

---

## 3. Decisões de projeto registradas durante o planejamento

Lista cronológica das decisões fechadas, para não precisar perguntar de novo durante a implementação:

| # | Decisão | Detalhe |
|---|---|---|
| 1 | Faixas finais dos 5 itens | Ver seção 2 e a tabela de faixas no `historico/`. |
| 2 | Validação cruzada 1↔2 | `mínimo_loop = N_cavidades_ativas × (led_capture_time + 0,5s)`. Recalculada a cada mudança em qualquer um dos dois campos ou no nº de cavidades ativas. Nunca usa o nº de cavidades fisicamente presentes no momento — sempre o nº de cavidades *habilitadas*. |
| 3 | Validação cruzada 2/3↔5 | `tempo_checagem_antecipada >= (N_iniciais + M_finais) × loop_cycle_time`. |
| 4 | Guard defensivo no firmware | `if (size < N+M) return false;` no início de `AmpouleSensor::get_test_result()` — proteção contra acesso a índice negativo, independente de qualquer validação feita na web. |
| 5 | Escopo da proteção CRC | Só os 5 campos novos por agora. `positive_percentage` e `calib_factor` continuam sem proteção adicional (decisão explícita — não expandir agora). |
| 6 | Persistência: escrita | Os 5 campos são gravados juntos como uma única struct/blob (`nvs_set_blob` + `nvs_commit` único, não 5 chamadas separadas). Sequência: grava Área 1 → lê e verifica → grava Área 2 → lê e verifica. Se a verificação falhar em qualquer uma das áreas, é erro imediato — sem retry automático, sem seguir adiante como se nada tivesse acontecido. |
| 7 | Persistência: leitura (boot) | Lê Área 1 (checa CRC) → lê Área 2 (checa CRC) → se ambos os CRCs ok e os valores forem iguais, usa os valores. Se qualquer CRC inválido OU valores diferentes entre as áreas (mesmo com CRC válido nos dois), é erro — **nenhuma área "vence" automaticamente**. |
| 8 | Comportamento durante erro de CRC | O equipamento aciona o alarme sonoro (contínuo/intermitente, mesmo padrão dos alarmes de temperatura já existentes — não é um beep único) e passa a operar imediatamente com os valores padrão de fábrica **em memória (RAM)**, sem travar o uso. Ao entrar na área protegida da web, mostra aviso de erro com botão para restaurar definitivamente o padrão de fábrica (grava nas duas áreas, encerra o alarme). |
| 9 | Botão "Restaurar padrão de fábrica" | Não exige senha adicional — estar dentro da área restrita (já protegida por senha) é suficiente. Só uma tela de confirmação (Confirmar / Voltar), sem reautenticação. |
| 10 | Botão "Resetar dispositivo" existente | Não é alterado. Continua com seu escopo atual (instituição, histórico de 12 testes, dados temporários) e seu próprio fluxo de senha — mecanismo totalmente separado do novo botão de restaurar padrão de fábrica dos 5 itens. |
| 11 | Valores padrão de fábrica dos 5 itens | Iguais ao comportamento atual do equipamento: captura 2s, loop 30s, amostras 5+5, 4 cavidades ativas, checagem antecipada 7 min — para quem já usa o equipamento hoje não perceber nenhuma mudança de comportamento até configurar algo diferente. |
| 12 | Bloqueio de "0 cavidades ativas" | Bloqueado — pelo menos 1 cavidade deve continuar ativa. Na interface: permite desmarcar a última, mas bloqueia o **Salvar** com mensagem de erro (não impede o clique no checkbox em si). |
| 13 | Bloqueio de salvar com teste em andamento | O(s) endpoint(s) novo(s) vão checar `ampoule_any()` e bloquear o salvamento se houver qualquer ampola em teste — trazendo a página nova para o mesmo padrão que Calibração e Reset já têm (e que o Restrict atual não tem). |
| 14 | Aviso de teste em andamento na tela | A página nova verifica `ampoule_any()` **assim que carrega** (não só no clique de Salvar) e já exibe um aviso bloqueando o uso, no mesmo padrão das páginas de Calibração e Reset. |
| 15 | Localização da nova configuração | Página nova (`/admin/advanced_config`), dentro do menu "Área Restrita", e não dentro da `restrict.html` existente — evita poluir uma tela de celular com muitos campos numa página que hoje só tem 1 campo. |
| 16 | Traduções | A página nova recebe traduções completas (pt-br / es-es / en-us), no mesmo padrão de internacionalização já usado pelas outras páginas. |
| 17 | Estratégia de branch | Trabalho feito em branch separada (`feature/config-web`), não direto no `main`. |
| 18 | Ritmo de implementação | Incremental, item por item, com build (via Docker) e revisão entre cada etapa — não tudo de uma vez. |

---

## 4. Correções feitas durante o planejamento (premissas iniciais que não bateram com o código)

Registradas para não repetir a mesma checagem depois:

1. **Item 1 (LED delay):** pedido original era 1 a 7 segundos; após análise eletrônica do sensor A/D, o piso seguro foi corrigido para **0,5 a 7 segundos**.
2. **Item 2 (looping):** o cliente relatou que o valor atual era 20 segundos; conferido no código (`TASK_TIME` + `TASK_TIME_DELAY`), o valor real é **30 segundos**. Não há indício, no histórico deste repositório git, de que já tenha sido 20s.
3. **Itens 3 e 4 (amostras):** o pedido original trazia dois números diferentes para a mesma função do sistema (5+5 vs. 7+8). Eram descrições da mesma funcionalidade; foram unificadas em uma única configuração (N iniciais / M finais, 3 a 10 cada).
4. **Item 5 (tempo mínimo de impressão):** hipótese inicial de que o sistema "não imprime se o ciclo for menor que 7 minutos" foi **checada e descartada** — não existe essa regra no código. O que existe é uma checagem de resultado positivo antecipado a partir de 7 minutos; testes negativos sempre seguem até o fim do ciclo total e imprimem normalmente.
5. **Item 4 (cavidades):** o cliente descreveu inicialmente como se a tela de checkboxes já existisse ("Na web eu tenho..."); confirmado depois que é uma funcionalidade nova, ainda não implementada.
6. **Padrão de senha nas áreas protegidas:** suposição de que Calibração, Reset e Restrict seguiam o mesmo esquema de senha foi **checada e corrigida** — são três mecanismos diferentes (ver seção 1.3).
7. **Bloqueio durante operação:** suposição de que nenhuma configuração podia ser alterada com teste em andamento foi **checada e corrigida** — só Calibração e Reset bloqueiam hoje; Restrict não bloqueia (por isso a decisão nº 13 acima, para os itens novos).

---

## 5. Arquivos já criados e implementados

- `components/advanced_config/` (novo componente) — struct `advanced_config_t`, defaults, carga/gravação em dupla área da NVS com CRC (`advanced_config_load/save/restore_defaults`).
- `components/httpd_app/www/pages/advanced_config.html` — página completa (itens 1-5 + item 6/temperatura, seção 7), traduzida (pt-br/es-es/en-us), gzipada e no `CMakeLists.txt`.
- `main/app_httpd.cpp` — rotas registradas (`GET /admin/advanced_config`, `GET/POST /api/v1/advanced_config`, `POST /api/v1/advanced_config/restore_defaults`), protegidas por `httpd_register_basic_auth` (autenticação **atualmente desativada** para testes, ver `historico/`).
- `main/ampoule_test.cpp`, `components/ampoule_sensor` — itens 1-5 usando `g_advanced_config` em vez de constantes fixas.
- `main/heater.cpp` — item 6 (temperatura) usando `g_advanced_config` em vez de `HEATER_TEMPERATURE`/`heater_min_temp`/`heater_max_temp`/`get_min_temperature()`/`get_max_temperature()` hardcoded.

## 6. Próximos passos

1. ~~Implementação dos itens 1-5~~ — feito (commit `85fab6e`), testado ao vivo (`historico/`, seção 9).
2. ~~Bug do `restore_defaults` não bloquear durante teste~~ — corrigido e testado ao vivo (`historico/`, seção 9).
3. Item 6 (temperatura) — implementado nesta sessão (seção 7 abaixo). **Ainda não gravado no equipamento nem testado ao vivo** — só compilado.
4. Reverter a autenticação desativada (`check_basic_auth()` em `app_httpd.cpp`) antes do uso normal do equipamento.
5. Atualizar o manual do usuário (`.docx`) com as telas de Configurações Avançadas (itens 1-6).
6. Confirmar com o cliente/responsável pela validação biológica os valores absolutos de temperatura (hoje 20-60°C é só um limite de engenharia provisório, ver seção 7.3) e a margem de 4°C definida para mínimo/máximo em relação ao setpoint.

---

## 7. Item 6 — Temperatura (pedido do cliente após a implementação inicial)

### 7.1 Estado encontrado no código (antes da mudança)

Levantamento feito analisando `main/heater.cpp` a pedido do cliente, que suspeitava haver "algum detalhe" na temperatura:

- `HEATER_TEMPERATURE = 37.0f` (constante) — único ponto de comparação do controle liga/desliga do aquecedor (`check_heater_temperature_task`): liga se `heater_temperature < 37.0`, desliga caso contrário. **Sem histerese** — mesmo valor exato liga e desliga.
- Duas faixas hardcoded **diferentes** e **independentes** do setpoint:
  - `get_min_temperature()=33` / `get_max_temperature()=43` — fora dessa faixa: cancela teste em andamento (`trigger_temp_out_of_range_cancel()`) + soa alarme (`heater_fail_start()`).
  - `heater_min_temp=35` / `heater_max_temp=43` — dentro dessa faixa: `check_if_heater_temperature_stabilized()` retorna true, liberando os botões/início de novo teste.
- `Kconfig.projbuild` tem `HEATER_TEMPERATURE` (default 60) e `HEATER_TEMPERATURE_PRECISION` (default 2) — **confirmado morto/sem efeito**, o código real ignora o Kconfig e usa a constante `37.0f` hardcoded (`//(float) CONFIG_HEATER_TEMPERATURE_PRECISION` comentado).
- Conclusão importante repassada ao cliente: a lógica de mínimo/máximo/liberação **não é** resquício de histerese abandonado — ela roda a cada leitura de temperatura (~1x/s) e tem efeito real (cancela teste, dispara alarme, libera botões). Não é um parâmetro "de conveniência" como os itens 1-5; mexe em segurança térmica do teste biológico.

### 7.2 Evolução da conversa até o design final (registrado para não repetir a discussão)

Propostas intermediárias consideradas e descartadas antes de chegar ao design final:
- Setpoint calculado como média entre um "máximo" e "mínimo" configurados, com regra de janela fixa (`máximo - mínimo = 8`) e um segundo par `alarme_mínimo`/`alarme_máximo` (`mínimo-2`/`máximo+2`). Descartada: ao mapear contra o código real, 2 dos 6 valores (`máximo` e `alarme_mínimo`, no exemplo usado) não correspondiam a nenhum uso real na lógica — o cliente confirmou que a ideia veio de "setups" genéricos de outros equipamentos, não de engenharia reversa do firmware real.
- Regra de margem mínima entre setpoint e mínimo/máximo passou por duas versões invertidas antes de chegar à final (`mínimo ≤ setpoint-4` / `máximo ≥ setpoint+4`, ambas por baixo — ou seja, uma margem **mínima** obrigatória de 4°C para cada lado, evitando janela apertada demais que gere alarme por oscilação normal do aquecedor).

### 7.3 Design final implementado

4 campos configuráveis (`components/advanced_config/include/advanced_config.h`, campos `heater_setpoint_c`, `heater_min_temp_c`, `heater_max_temp_c`, `heater_release_temp_c`):

| Campo | Substitui (hoje hardcoded) | Efeito real |
|---|---|---|
| `heater_setpoint_c` | `HEATER_TEMPERATURE` (37) | Ponto único de liga/desliga do aquecedor (sem histerese, comportamento preservado) |
| `heater_min_temp_c` | `get_min_temperature()` (33) | Abaixo disso: cancela teste em andamento + alarme |
| `heater_max_temp_c` | `get_max_temperature()`/`heater_max_temp` (43, mesmo valor hoje) | Acima disso: cancela teste em andamento + alarme; também teto da faixa "estabilizada" |
| `heater_release_temp_c` | `heater_min_temp` (35) | Piso da faixa "estabilizada" (libera botões/início de teste) — **calculado na tela** como `mínimo + 2`, campo `readonly` |

Regras de validação (defensivas no backend, `main/app_httpd.cpp`, `api_advanced_config_post_handler`):
1. Faixa absoluta 20-60°C para os 4 campos — **provisória, só para barrar valores absurdos**; não é uma faixa clinicamente validada (precisa confirmação de quem valida o método biológico, decisão nº 6 da seção 6 acima).
2. `setpoint - mínimo >= 4` e `máximo - setpoint >= 4` (margem mínima de 4°C, inclusive).
3. `liberação == mínimo + 2` (exato, tolerância de 0,01 por ser float).

Valores padrão escolhidos para **não mudar o comportamento atual** do equipamento: setpoint=37, mínimo=33, máximo=43, liberação=35 — idênticos aos valores hardcoded que substituem.

Arquivos alterados: `components/advanced_config/include/advanced_config.h` (struct), `components/advanced_config/advanced_config.cpp` (defaults, log), `main/heater.cpp` (usa `g_advanced_config` em vez das constantes/globais antigos), `main/app_httpd.cpp` (GET/POST), `components/httpd_app/www/pages/advanced_config.html` (novo card "Temperatura"), traduções (`pt-br`/`es-es`/`en-us`.json).

**Pendente:** gravar o binário novo no equipamento e repetir a bateria de testes ao vivo (salvar válido, rejeições de faixa, bloqueio durante teste) que já foi feita para os itens 1-5.
