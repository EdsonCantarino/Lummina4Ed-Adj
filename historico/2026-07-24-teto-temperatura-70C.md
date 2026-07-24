# Mudanças desta versão — Teto de temperatura 60°C → 70°C

**Data:** 24/07/2026
**Branch:** `feature/config-web`
**Motivo:** bateria de testes ao vivo (COM20) exigiu configurar mínima=55°C / liberação=57°C / setpoint=60°C / máxima=65°C — combinação rejeitada pelo teto absoluto de engenharia de 60°C definido no item 6 (ver `historico/2026-07-23-configuracoes-web.md`, seção 10).

## Alterado

- `main/app_httpd.cpp` — `HEATER_TEMP_ABS_MAX` de `60.0f` para `70.0f` (validação do `POST /api/v1/advanced_config`, ainda um limite de engenharia provisório, não clinicamente validado).
- `components/httpd_app/www/pages/advanced_config.html` — atributo `max` dos 4 campos de temperatura (`heater_setpoint`, `heater_min_temp`, `heater_max_temp`, `heater_release_temp`) de `60` para `70`, para o front-end não bloquear antes de chegar no backend.

## Não alterado

- `HEATER_TEMP_ABS_MIN` (20°C) — mantido.
- Regras cruzadas (margem mínima de 4°C entre setpoint e cada extremo; liberação = mínima + 2 exata) — mantidas sem mudança.

## Também nesta sessão (fora do código, achado durante o teste)

- Fator de calibração de temperatura salvo na NVS do equipamento em teste estava em **-13°C**, mascarando um superaquecimento real do bloco (~50°C reportado como ~36,5°C). Zerado via `POST /api/v1/device/calibration` durante o teste. **Isso é dado do equipamento, não do firmware** — não é uma mudança de código, mas fica registrado aqui porque motivou a mudança de teto acima.

## Pendente

- ~~Recompilar + regravar (COM20) com o teto de 70°C.~~ Feito nesta sessão.
- ~~Gravar a configuração final (mínima=55 / liberação=57 / setpoint=60 / máxima=65) pela tela `/admin/advanced_config`.~~ Feito nesta sessão (via API, mesmo efeito da tela).
- Confirmação de quem valida o método biológico/clínico sobre essa faixa 55-65°C (mesma ressalva já registrada no item 6 para a faixa 20-60°C original).

## IMPORTANTE — reverter antes de produção: autenticação da área restrita está desligada

`main/app_httpd.cpp`, função `check_basic_auth()` (linha ~169), tem um `return ESP_OK;` logo no início que **desliga completamente o Basic Auth** de toda a área restrita (`/admin/restrict`, `/admin/advanced_config`, `/admin/serialnumber`, monitoramento de ampolas). Foi commitado no `e3a2a37` (23/07) como "TESTE TEMPORARIO" para não travar a automação do navegador (diálogo nativo de Basic Auth do Chrome) durante o debug da tela de configuração avançada — comentário no próprio código já diz "REVERTER antes de voltar o equipamento para uso normal".

**Decisão (24/07):** mantido desligado durante a bateria de testes. **Revertido no fim da sessão (24/07, tarde)** — `return ESP_OK;` removido de `check_basic_auth()`, autenticação normal restaurada, recompilado e regravado.

## IMPORTANTE — reverter antes de produção: buzzer silenciado

`main/buzzer.cpp`, função `buzzer_on()`, tem um `return;` logo no início que **silencia todo o buzzer** (alarmes de temperatura, alarme de cancelamento de teste, etc.) — usuário pediu porque estava testando por vídeo-chamada em reunião e o beep atrapalhava. **Revertido no fim da sessão (24/07, tarde)** — `return;` removido de `buzzer_on()`, buzzer normal restaurado, recompilado e regravado.

## Nova UX de Data/Hora (RTC) + bugs encontrados e corrigidos na mesma sessão

**Motivo:** ticket impresso pela ampola 1 mostrou hora errada (RTC do equipamento adiantado, chegou a ~35 min de diferença do horário real ao longo da sessão — cristal do DS1302 parece estar rodando rápido; pode precisar trocar a pilha/módulo). A tela antiga (`settings.html`) só mostrava a hora do PC e sobrescrevia o RTC sem perguntar nem mostrar o valor atual.

**Mudanças:**
- `main/app_httpd.cpp`: `GET /api/v1/settings` agora retorna `deviceDateTime` (leitura real do RTC via `rtc_ds1302_get_date_time()`).
- `settings.html`: novo campo somente-leitura com a hora atual do RTC, campo editável pré-preenchido com o valor do RTC (não mais com a hora do PC), botão "Atualizar hora do Lummina?" (preenche com a hora do navegador só se clicado) e modal de confirmação mostrando RTC atual vs. novo valor antes de gravar.

**Bug 1 — `.html.gz` desatualizado (achado ao testar no navegador):** `components/httpd_app/CMakeLists.txt` usa `EMBED_FILES` apontando direto para os `.html.gz` — eles **não são gerados automaticamente** a partir do `.html` pelo build do CMake/Docker. 5 de 7 páginas estavam com `.gz` mais antigo que o `.html` correspondente, incluindo `advanced_config.html` (mudança do teto 70°C) e `settings.html` (RTC) desta mesma sessão — ou seja, ambas as mudanças de UI de hoje nunca tinham ido pro ar antes dessa correção, só o backend C++ (compilado do zero sempre) estava atualizado. Corrigido regenerando `advanced_config.html.gz`, `ampoules.html.gz`, `restrict.html.gz`, `serialnumber.html.gz`, `settings.html.gz` com `gzip -kf9`. **Processo continua manual — sempre rodar `gzip -kf9 <arquivo>.html` (e os `.json` de idioma, se mudados) depois de editar qualquer página, antes de compilar.**

**Bug 2 — botão invisível:** `btn-outline-secondary` não renderiza legível nesse bundle customizado do Bootstrap (texto quase branco sobre fundo claro). Trocado para `btn-secondary` (mesma classe já usada com sucesso em `advanced_config.html`).

**Bug 3 — tradução cacheada:** `jquery.multilanguage` salva o JSON de tradução no `localStorage` do navegador (`getTranslate`/`saveTranslate`) e só busca do servidor de novo se o cache estiver vazio — mudanças no `.json` de idioma não aparecem até limpar `localStorage` (comportamento existente do sistema, não é bug introduzido agora, só ficou mais visível testando).

**Bug 4 — `httpd_req_recv` com tamanho errado, pré-existente desde o commit inicial (`7836c58`):** em 8 handlers POST de `main/app_httpd.cpp` (linhas 569, 968, 1069, 1133, 1297, 1546, 1623, 1678), o loop de recebimento do corpo da requisição chamava `httpd_req_recv(req, buf + cur_len, total_len)` — sempre pedindo o tamanho **total**, não o **restante** (`total_len - cur_len`). Quando o corpo chega fragmentado em mais de um pacote TCP (comum em requisição de navegador, com cabeçalhos maiores que uma chamada simples via `curl`/PowerShell), a segunda leitura pede mais bytes do que restam e o ESP-IDF recusa, retornando erro — sintoma: "Erro ao salvar os dados. Tente novamente!" no navegador, mesmo com o payload correto. Corrigido em todos os 8 pontos para `total_len - cur_len`. **Isso afetava todo endpoint POST do equipamento (calibração, configurações avançadas, restrict, data/hora, etc.), de forma intermitente — vale ficar atento a esse mesmo sintoma em qualquer tela daqui pra frente.**

**Testado e confirmado funcionando (24/07, via navegador real):** menu "Configurações Avançadas" aparece na Área Restrita; tela de temperatura mostra 55/57/60/65 corretamente; tela de Data/Hora mostra RTC + campo editável + botão + modal de confirmação; salvar data/hora funciona (RTC do equipamento atualizado com sucesso).
