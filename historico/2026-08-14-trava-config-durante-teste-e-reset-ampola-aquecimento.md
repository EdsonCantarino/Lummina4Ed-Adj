# Checkpoint 14/08 — trava de configuração durante teste + reset por ampola durante aquecimento

**Data:** 14/08/2026
**Branch:** `feature/config-web`
**Status:** as duas mudanças abaixo compilam limpo (`Lummina4_MAXXIMED_v2026_08_14.bin`), **nada gravado no equipamento ainda, nada commitado**. Usuário pediu pra não gravar nesta sessão.

## 1. Trava de configuração durante teste em andamento

**Motivo:** cliente relatou que conseguiu alterar o número de série pela web com uma ampola em teste. Regra pedida: nenhuma alteração via webserver deve ser permitida enquanto houver análise em andamento — como regra geral, não só pro número de série.

**Levantamento:** de 11 handlers POST em `main/app_httpd.cpp`, 5 não tinham a trava `ampoule_any()` (padrão já usado em Configurações Avançadas/Calibração/Reset/Histórico):
- `POST /api/v1/restrict/serialnumber` — o mais crítico: em caso de sucesso chama `restart_device()`, ou seja, reiniciaria o equipamento no meio de um teste.
- `POST /api/v1/device/settings` (Data/Hora Servidor + instituição)
- `POST /api/v1/restrict/settings` (percentual positivo)
- `POST /api/v1/buzzer_config` (tempo do alerta do buzzer)
- `POST /api/v1/language` (idioma da UI — perguntado ao usuário se bloqueia também, confirmado que sim, seguindo a regra geral)

**Fix:** guard `ampoule_any()` adicionado nos 5, com um helper novo `send_test_in_progress_error()` (reaproveita `send_history_error()` pro buzzer, que já estava depois na ordem do arquivo). Mensagem padrão: "Não é possível alterar essa configuração enquanto houver análises em andamento."

**Bug de front-end encontrado no processo:** `serialnumber.html`, `settings.html` e `restrict.html` mostravam toast de "sucesso" mesmo quando o backend recusava (`{"success": false}`) — não checavam `response.success`, ao contrário de `advanced_config.html`/buzzer, que já faziam isso certo. Corrigido nos 3 pra checar `response.success` e mostrar a mensagem de erro real. `.gz` das 3 páginas regenerado.

**Não bloqueado de propósito:** `/api/v1/translate` (só serve o JSON de tradução, não persiste nada).

## 2. Reset ao ligar/inserir ampola durante o aquecimento

**Motivo:** cliente pediu que, ao ligar com ampola já inserida, o equipamento faça um reset — mas também disse querer "igual ao comportamento anterior" (firmware original, `D:\Github\ECK\Maxximed\Lummina4Ed`, sem "Adj"). As duas coisas pareciam contraditórias a princípio.

**Investigação do firmware original (registrada em detalhe na conversa, resumo aqui):**
- A trava de boot do original (`is_ampoules_present_in_init`, `main/ampoules.cpp`/`ampoule_sensor.cpp` do projeto sem "Adj") nunca reseta — só trava botões + alarme contínuo (beep+LED) até a ampola ser removida, liberação instantânea (sem debounce). Esse comportamento já existe hoje no projeto atual (Adj), só que com uma janela de confirmação de 10s (`AMPOULE_ABSENT_CONFIRM_MS`) em vez de instantâneo — criada pra condição de impressora travada, **usuário confirmou não mexer nela**.
- Só numa segunda leitura mais cuidadosa (o usuário corrigiu minha primeira análise, que estava incompleta) achei o mecanismo que realmente reseta no original: em `ampoules.cpp`, `check_temperature_status(true)` — se tem qualquer ampola presente (`ampoule_any()`) e a temperatura está fora de `[setpoint-10, setpoint+8]`, chama `esp_restart()`. Como o equipamento liga em temperatura ambiente, isso dispara quase sempre que alguém insere uma ampola antes do aquecedor esquentar o suficiente — cobre tanto "ligar com ampola" quanto "inserir durante o aquecimento".
- Existe um mecanismo parecido mas **diferente** em `heater.cpp`, gated por `is_any_testing()` (teste já em andamento) + faixa absoluta 50-68°C — esse já foi removido de propósito no projeto atual (comentário "Removido: esp_restart()... evitando reboot indevido", presente desde o commit inicial deste repo `7836c58`, antes de qualquer sessão documentada). Esse é sobre teste comprometido no meio do caminho, não sobre o processo de aquecimento — não é o mecanismo relevante aqui, e não foi mexido.

**Desenho final acordado com o usuário:**
- Critério: ampola presente (nas 4 cavidades, **mesmo desabilitadas** — decisão explícita do usuário) **e** temperatura ainda abaixo de `heaterReleaseTemp` ("liberação para trabalho", já configurável) → reset.
- Não usa `ampoule_any()` (que ignora cavidade desabilitada, `is_present` fica congelado nela) — usa a leitura bruta já existente `check_if_ampoules_is_confirmed_present_in_init()` (`ampoule_sensor.cpp`), que é atualizada sem parar, independente do estado da trava de boot.
- Sem risco de loop: o bloco novo só é alcançado depois que a trava de boot de 10s já liberou (fica antes dele no fluxo, com `continue`). Se resetar e a ampola continuar presente, no boot seguinte quem assume é a trava de boot (só alarma, não reseta de novo).

**Implementado em `main/ampoules.cpp`** (`read_ampoules_test_task`), log `RESTART_ID=5` (próximo número livre da convenção: 1=printer, 2=restart_device, 3=reset_user_data, 4=DRDY timeout).

## Validação física — concluída (14/08, mesmo dia)

Todos os itens pendentes acima foram confirmados no equipamento (COM20):

- **Trava de config**, os 5 endpoints (número de série, data/hora+instituição, buzzer, % positivo, idioma), testados via Chrome com teste real em andamento (modo ETO, estabilizado) — todos bloqueados corretamente, incluindo o caso crítico do número de série (sem reiniciar o equipamento). Detalhe completo em `historico/2026-08-14b-validacao-fisica-trava-config-e-presets-modo.md`.
- **Reset por ampola/aquecimento**: confirmado disparando ao inserir ampola durante o aquecimento (cavidade habilitada), sem loop. **Confirmado também com cavidade bloqueada pelo CRC1** — o usuário testou trocando pra CRC1 e validou que o reset dispara mesmo com as cavidades 2-4 travadas, conforme decisão de design (tratar todas as cavidades igual nessa fase).

Commitado e pushado em `7a3b247` (código) + commit desta validação (ver log do git).
