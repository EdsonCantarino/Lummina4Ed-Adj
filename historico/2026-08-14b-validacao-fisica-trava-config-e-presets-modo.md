# Checkpoint 14/08 (parte 2) — validação física da trava de config e dos presets por modo

**Data:** 14/08/2026
**Branch:** `feature/config-web`
**Commit da sessão:** `7a3b247` (pushed antes desta validação)
**Ambiente:** equipamento em COM20, AP `MAX-C3B828`, modo ETO com teste real em andamento (ampola aquecida, temperatura estabilizada em 36,5°C, range 30-44) — usado de propósito pelo usuário pra habilitar o teste da trava.

## 1. Trava de configuração durante teste em andamento — validado nos 5 endpoints

Testado pelo Chrome (automação), com o log serial (COM20) como fonte de verdade (`app_httpd: Alteracao recusada: analise em andamento`), já que o toast de erro na tela nem sempre foi capturado por instabilidade do screenshot da extensão nesta sessão:

- **Número de série** (`serialnumber.html`) — o bug original relatado pelo cliente. Preenchido com o mesmo valor atual e clicado Salvar: recusado, **sem reiniciar o equipamento** (confirmado: nenhum `rst:0x` no log perto do evento). Toast de erro "Não é possível alterar essa configuração enquanto houver análises em andamento." confirmado visualmente também.
- **Data/Hora Servidor + Instituição** (`settings.html`, topo) — recusado, RTC não mudou.
- **Buzzer** (`settings.html`, alerta sonoro) — recusado.
- **Percentual de positivação** (`restrict.html`) — recusado, toast de erro confirmado visualmente (screenshot limpo).
- **Idioma** (dropdown "Idioma"/"Language", qualquer página) — recusado no backend (log confirma), mas a troca visual acontece do mesmo jeito no navegador (client-side, via `localStorage`, independente do resultado do POST) — comportamento esperado, já sabido de antemão.

Nenhum dos 5 alterou o equipamento de fato durante o teste. Revertido o idioma pra pt-br ao final (só o display local, nada gravado).

## 2. Presets por modo (Normal/ETO/CRC1) — validado via inspeção direta do estado JS

Testado direto no `/admin/advanced_config` (não deu pra salvar de propósito, já que havia teste em andamento — mas a troca de modo só mexe nos campos da tela, não salva sozinha). Em vez de confiar em screenshot (instável nesta sessão, vários timeouts de captura da extensão), os valores foram lidos direto via `javascript_tool` (`$('#campo').val()`), método mais confiável.

| Campo | Normal (lido) | ETO (lido) | CRC1 (lido) | Esperado |
|---|---|---|---|---|
| LED | 0,5 | 0,5 | 0,5 | ✅ |
| Looping | 4 | 4 | 2 | ✅ |
| Amostras | 5+5 | 5+5 | 5+5 | ✅ |
| Checagem antecipada | 3 | 3 | 1 | ✅ |
| Setpoint | 60 | 37 | 60 | ✅ |
| Mínima | 53 | 30 | 53 | ✅ |
| Máxima | 67 | 44 | 67 | ✅ |
| Liberação (auto) | 55 | 32 | 55 | ✅ (ETO: 32, não os 33 que o cliente tinha passado — decisão já tomada de usar a regra mínima+2) |
| Cavidades | todas | todas | só 1 (travada) | ✅ |

Todos os 9 campos batendo exatamente com a tabela de presets definida na sessão anterior, nos 3 modos. CRC1 também confirmado travando/destravando corretamente as cavidades 2-4 ao entrar/sair (`cav2disabled: true` em CRC1, `false` ao voltar pra ETO).

## 3. Reset por ampola/aquecimento em cavidade bloqueada pelo CRC1 — validado

Depois da validação dos presets, o usuário trocou o equipamento pra CRC1 de verdade e testou o reset (item 2 do checkpoint anterior) com uma cavidade travada pelo modo — confirmou que dispara igual, batendo com a decisão de tratar todas as 4 cavidades igual nessa fase (independente de habilitada/desabilitada).

## Pendente

- Nada pendente. Todos os itens desta sessão e da sessão anterior (`historico/2026-08-14-...md`) confirmados fisicamente.

## Observação técnica (fora do escopo, registrada por precaução)

A extensão do Chrome (`claude-in-chrome`) apresentou instabilidade nesta sessão — vários timeouts de `Page.captureScreenshot` (30s) em `192.168.10.10`, com a página respondendo normalmente a `get_page_text`/`javascript_tool` em paralelo. Não pareceu ser problema do firmware (a página sempre respondia normalmente a chamadas JS diretas) — mais provável ser da extensão/CDP nesta sessão do navegador. Se voltar a acontecer, preferir `javascript_tool`/`get_page_text` a `screenshot` para verificação de estado.
