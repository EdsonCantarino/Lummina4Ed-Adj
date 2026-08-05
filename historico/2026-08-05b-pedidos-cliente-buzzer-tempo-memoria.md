# Pedidos do cliente (05/08, tarde/noite) — botão buzzer, defaults de fábrica, cache de tradução, breadcrumb reset (+ tentativa revertida de memorizar tempo)

**Data:** 05/08/2026
**Branch:** `feature/config-web`
**Status:** Botão buzzer, defaults de fábrica, cache de tradução e breadcrumb do reset implementados, compilados e gravados no equipamento de testes (COM20). **Nada commitado no git ainda** — aguardando confirmação do usuário na próxima sessão. A feature "memorizar último tempo por cavidade" foi implementada, debugada extensivamente, e **revertida por completo a pedido do usuário** (ficou complicada demais pro ganho) — ver seção 3 abaixo.

## Contexto

Sessão de continuação depois do fix de label flutuante da manhã ([`2026-08-05-fix-sobreposicao-label-flutuante.md`](2026-08-05-fix-sobreposicao-label-flutuante.md)). O cliente foi passando uma lista de pedidos novos, um de cada vez, discutindo antes de cada implementação.

## 1. Botão do buzzer na membrana

Pedido: "Limitar a função do botão do buzzer na membrana: fazer com que ele apenas desligue o buzzer quando estiver tocando, sem permitir que ele seja ligado manualmente pelo botão."

Hoje o botão fazia um beep de 100ms + toggle do estado `is_buzzer_on_off` em **todo** clique, mesmo sem alarme tocando — dava a impressão de "ligar" o som.

**Fix:** `main/keyboard.cpp` (bloco `BUTTON_SOUND_OFF`) — botão só age (beep + desliga) quando `is_buzzer_on_off && is_buzzer_alert_on_off` (alarme realmente tocando). Sem alarme ativo, clique fica inerte. Novo getter `get_buzzer_alert_on_off_status()` em `main/buzzer.cpp`/`main/include/buzzer.h`.

**Não confirmado fisicamente ainda.**

## 2. Configuração default de fábrica

Cliente mandou 3 prints (`C:\Users\Edson\Downloads\Default\`) com a configuração que ele quer como padrão de fábrica: 4 cavidades, checagem antecipada 3min, modo Normal, setpoint 60°C/mín 53°C/máx 68°C, LED 0,5s, looping 4s, 5+5 amostras.

**Fix:** `components/advanced_config/advanced_config.cpp`, struct `ADVANCED_CONFIG_DEFAULTS`. Aplicado tanto no primeiro boot quanto em "Restaurar padrão de fábrica" (mesma constante, um só lugar pra mudar).

**Não confirmado fisicamente ainda.**

## 3. Memorizar último tempo selecionado por cavidade — IMPLEMENTADO E DEPOIS REVERTIDO

Pedido: hospitais que usam sempre o mesmo ciclo (ex.: 3h) não deveriam ter que reconfigurar o tempo toda vez que o equipamento é desligado.

**Resultado final: revertido por completo, a pedido do usuário** ("estou achando isso muito complicado... consegue remover essa função de memória da última corrida de ampola e deixar isso do jeito que estava, ou seja, sempre parte de 20M?"). Os 5 arquivos tocados por essa feature (`main/nvs_utils.cpp`, `main/include/nvs_utils.h`, `main/ampoule_test.cpp`, `main/include/ampoule_test.h`, `main/app_httpd.cpp`) foram restaurados via `git checkout` pro estado de início de sessão — nenhum deles tinha mudança de outra feature misturada, então o revert foi limpo. Comportamento do equipamento voltou a ser exatamente o de antes de hoje: `init_ampoules()` sempre força `DEFAULT_TIME_TEST` (5min) no boot, sem memorizar nada.

**Por que não emplacou** (documentado pra não repetir o mesmo caminho numa tentativa futura, se o cliente pedir de novo):

### Desenho acordado com o usuário
- 4 variáveis persistidas (uma por cavidade), não um valor global.
- Grava só ao **final de um ciclo completo** (não a cada clique de navegação no botão) — `finalize_ampoule_test()`.
- Reseta pro default (20min — "primeiro tempo") ao Salvar ou Restaurar Padrão de Fábrica em Configurações Avançadas, e também ao "Resetar dispositivo" (pra não deixar preferência de hospital anterior presa no equipamento).
- LED do tempo memorizado acende quando a temperatura estabiliza.

### Implementação
- `main/nvs_utils.cpp`/`.h`: `save_ampoule_time_state()`/`get_ampoule_time_state()`, chaves `time_st_1`..`time_st_4`, escala própria 1=20min(default)/2=1h/3=3h/4=5min.
- `main/ampoule_test.cpp`: conversões estado↔segundos↔pino do LED; `init_ampoules()` carrega do NVS; `finalize_ampoule_test()` grava; `ampoule_reset_time_memory_to_default()` novo; `ampoule_test_check_cavity_finalize()` acende o LED certo por cavidade.
- `main/app_httpd.cpp`: chama o reset nos dois handlers (Salvar e Restaurar Defaults) — não dá pra centralizar dentro de `advanced_config_save()` porque esse componente é isolado do `main` (ver `components/advanced_config/CMakeLists.txt`, só `REQUIRES nvs_helpers`).
- `main/nvs_utils.cpp` (`reset_user_data()`): também zera as 4 chaves.

### Depuração física (a parte mais trabalhosa)

Depois de gravar, o painel mostrou LEDs errados/inconsistentes entre cavidades (ex.: cavidade 4 com LED43+LED45 vermelho aceso; depois cavidades 2/3/4 em "1H" e só a 1 em "20M"). Processo de diagnóstico:

1. **1ª hipótese (errada):** confundi com o desenho de "só salva ao final do ciclo" — usuário corrigiu, não tinha rodado teste nenhum.
2. **2ª hipótese (parcialmente certa):** nível 4 (5min) usa o mesmo pino que o LED de status verde/vermelho — risco real (pino compartilhado, mitigado por sequenciamento de uso), mas não a causa raiz dos sintomas observados.
3. **Causa raiz parcial #1 (confirmada):** `xTaskNotify` com `eSetBits` é uma **palavra de notificação única, não uma fila** — múltiplas chamadas em sequência sem delay se misturam via OR bit a bit antes da task consumidora (`led_panel_task_notify`, ciclo de ~100ms) processar cada uma. Fix: 150ms de delay entre cavidades.
4. **Tentativa errada:** cheguei a "corrigir" o pino pra `nível - 1`, baseado numa leitura de foto ambígua (achei que P0 = "20M"). **O usuário me corrigiu com um argumento decisivo: 7000 unidades desse equipamento rodam 12h/dia com o botão físico funcionando perfeitamente — se a convenção "pino = nível direto" (sem -1) estivesse errada, isso nunca teria funcionado em campo.** Fui checar `D:\Github\ECK\Maxximed\Lummina4Ed` (versão antiga sem a pasta "Adj", funcionando) e o usuário testou o botão físico ao vivo: sequência real confirmada é 20M(inicial)→1H→2H→3H→20M(volta), ou seja **pino = nível, sem nenhum ajuste** — minha primeira implementação (antes de eu mesmo "consertar" errado) já estava certa.
5. **Causa raiz parcial #2 (a real explicação de "cavidade 1 certa, as outras erradas", achada só depois de reler o código com calma):** `ampoule_test_check_cavity_finalize()` já era chamada **duas vezes por estabilização** no código original (`heater.cpp`, antes e depois de suspender a animação de aquecimento `blink_led_test_cavities`) mais uma vez independente em `ampoules.cpp` (flag de guarda separada, não sincronizada). Antes de mexer nisso, a função era instantânea (1 escrita síncrona) e essa sobreposição nunca importava; meu código novo (~600ms por causa do delay de 150ms × 4 cavidades) abriu uma janela grande o suficiente pra chamadas concorrentes de tasks diferentes se entrelaçarem. Fix: mutex (`xSemaphoreCreateMutex`) serializando as chamadas.
6. **Resultado após o fix do mutex: sintoma mudou mas não sumiu** — as 4 cavidades passaram a mostrar o **mesmo** valor errado (uma posição acima do esperado), em vez de só 3 delas. Ou seja, o mutex resolveu a inconsistência *entre* cavidades mas não a causa de um deslocamento uniforme. Não cheguei a identificar essa causa - a esse ponto o usuário pediu pra simplesmente reverter a feature inteira.

**Decisão final:** a complexidade acumulada (concorrência entre duas tasks diferentes chamando a mesma função, mais o mecanismo de notificação de LED que não é fila, mais a animação de aquecimento que escreve nos mesmos pinos) tornou a depuração cara demais pro valor da feature. Revertido por completo - ver início desta seção.

## 4. Cache de tradução do navegador

Durante a investigação do pedido "2" original da lista (texto da tela de reset), descobri que o texto já estava corrigido no firmware desde `d42b6cf` (03/08), mas o navegador mostrava versão antiga mesmo em equipamento recém-gravado. Causa: `jquery.multilanguage.min.js` cacheava a tradução em `localStorage` indefinidamente, nunca comparando com o que o firmware realmente serve.

**Fix:** removida a lógica de cache (`saveTranslate`/`getTranslate`); `getLanguage()` sempre busca fresco de `/api/v1/translate` (dado local embutido no firmware, custo desprezível numa rede local). Afeta as 8 páginas que incluem esse arquivo.

## 5. Breadcrumb da tela de reset

A tela `/reset` mostrava breadcrumb "Configurações / Dispositivo" — texto idêntico (copy-paste) ao da tela de configurações de verdade, nas 3 línguas. Corrigido pra "Configurações / Resetar Dispositivo" (`pt-br.json`, `en-us.json`, `es-es.json`, `reset.html`).

## Também feito

- Regeração de `.gz` de todas as páginas "por segurança" (nenhuma desatualizada encontrada, mas confirmado).

## Ambiente

- Build: `.\compila_Lummina4EdAdj.ps1` (Docker `espressif/idf:release-v5.1`).
- Flash: `esptool -p COM20 ...` (ver comando completo no script/histórico anterior).
- **Novo:** apagar NVS antes de cada flash de teste — `esptool -p COM20 -b 460800 erase_region 0x9000 0x6000`.
- Sempre matar processo miniterm antes de flashar (`Get-CimInstance Win32_Process | Where CommandLine -like '*miniterm*' | Stop-Process -Force`) — atenção: o PID retornado pelo `$!` do bash ao rodar `python -m serial.tools.miniterm ... &` dentro do wrapper do Bash tool **não é o PID real do processo python**; usar `Get-CimInstance` pra achar o PID de verdade.

## Pendente pra próxima sessão

- Confirmar fisicamente itens 1 (botão buzzer) e 2 (defaults de fábrica).
- Perguntar ao usuário se pode commitar/pushar as mudanças desta sessão.
- Cliente tem mais itens na lista original de pedidos, ainda não discutidos (conversa foi interrompida pela investigação de bugs do item 3).
- **Item 3 (memorizar tempo por cavidade) segue como pedido em aberto do cliente** - foi revertido nesta sessão por complexidade, não por decisão de que a feature não é desejada. Se for retomado no futuro, ler a seção 3 acima inteira antes de tentar de novo (principalmente a descoberta da chamada dupla concorrente de `ampoule_test_check_cavity_finalize()` e o mapeamento de pino confirmado com o botão físico).
