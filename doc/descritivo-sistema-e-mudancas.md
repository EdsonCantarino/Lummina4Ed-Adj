# Lummina 4 — Descritivo do Sistema e Registro de Mudanças

**Data de referência:** 23/07/2026
**Status:** Planejamento concluído, implementação ainda não iniciada (branch `feature/config-web` criada, aguardando autorização para começar).
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

## 5. Arquivos já criados (fase de planejamento / scaffold)

- `components/httpd_app/www/pages/advanced_config.html` — esqueleto da página nova (estrutura, campos, menu de navegação apontando pra si mesma, textos ainda não conectados a nenhum backend). **Ainda não registrado no servidor** (`app_httpd.cpp`), **não gzipado**, **não adicionado ao `CMakeLists.txt`**, e o **link no menu das outras 5 páginas ainda não foi adicionado** (aguardando autorização para prosseguir).

## 6. Próximos passos (aguardando autorização explícita para começar)

1. Adicionar o link "Configurações Avançadas" no menu das 5 páginas existentes.
2. Adicionar as traduções (es-es, en-us) da nova página.
3. Registrar a rota `/admin/advanced_config` em `app_httpd.cpp` (com `check_basic_auth`), gerar o `.gz` e incluir no `CMakeLists.txt`.
4. Implementar a struct de configuração + gravação em blob único + dupla área + CRC (`nvs_utils.cpp` / `nvs_helpers`).
5. Implementar a lógica de cada um dos 5 itens no firmware (`ampoule_test.cpp`, `AmpouleSensor.h`), incluindo os guards defensivos e a separação de constantes.
6. Sincronizar a lógica duplicada em `ampoules.html` (JS) com os novos N/M configuráveis.
7. Testar compilação via Docker a cada etapa.
