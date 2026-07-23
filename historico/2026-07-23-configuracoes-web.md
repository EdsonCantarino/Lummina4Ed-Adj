# Histórico técnico — Configurações via Web (5 itens)

**Data de referência:** 23/07/2026
**Origem:** Levantamento de requisitos do cliente + análise de código atual, discutido em sessão de planejamento antes de qualquer implementação.
**Status:** Nenhuma linha de código foi alterada ainda. Este documento é a referência técnica para quando a implementação começar.
**Documento não técnico correspondente:** `Proposta_Configuracoes_Web.txt` (raiz do projeto), gerado para o cliente a partir desta mesma análise.

---

## 0. Contexto geral

Todos os 5 itens abaixo serão configuráveis pela mesma área protegida da web que já existe hoje (`components/httpd_app/www/pages/restrict.html`, rota `/admin/restrict`), onde já se configura o `positive_percentage`. O padrão de persistência hoje (`main/nvs_utils.cpp` + `components/nvs_helpers`) grava cada campo isoladamente, com `nvs_commit` individual por chamada (`NVSHelper::setFloat/setInt(..., forceCommit=true)`), sem redundância nem checagem de integridade além do que a própria NVS do ESP-IDF já oferece internamente.

**Decisão de arquitetura para os 5 campos novos:** serão gravados como uma única struct/blob (uma chamada `nvs_set_blob` + `nvs_commit`, não 5 chamadas separadas), replicada em duas chaves NVS distintas ("área A" e "área B"), cada uma com CRC. Ver seção 6.

---

## 1. Tempo de captura da leitura (LED aceso antes da leitura)

**Onde está hoje:** `main/ampoule_test.cpp:25` — `#define TASK_TIME_DELAY 2000` (2000ms fixos), usado em `prepare_test()` (`ampoule_test.cpp:416-424`):

```cpp
void prepare_test(int ampoule) {
    led_uv_on(ampoule);
    vTaskDelay(pdMS_TO_TICKS(TASK_TIME_DELAY));
    set_heater_controlling(false);
}
```

**Mudança:** `TASK_TIME_DELAY` deixa de ser uma constante fixa e passa a ser uma variável configurável (persistida na NVS), faixa **0,5 a 7 segundos**.

**Implicação técnica importante:** hoje essa mesma constante também é somada a `TASK_TIME` (28000ms) em três outros lugares do mesmo arquivo:
- `ampoules_test_timer_task` (linha ~809): cálculo do delay de reinício do ciclo.
- Incremento de `current_time` em `ampoule_test()` (linhas ~651-656): usado para rastrear quanto tempo de incubação já se passou (compara com os 20 minutos totais do teste).

**Isso significa que não dá para simplesmente trocar o valor de `TASK_TIME_DELAY`** sem separar essas duas responsabilidades em duas variáveis distintas:
- `led_capture_time` (novo, configurável, 0,5–7s) — usado só em `prepare_test()`.
- `loop_cycle_time` (item 2 abaixo) — usado no cálculo do delay de reinício e no incremento de `current_time`.

**Piso de 0,5s (não 1s, como no pedido original do cliente):** justificado pela análise do sensor A/D (`light_sensor.cpp`), ver seção 2 — o piso puramente eletrônico defensável fica entre 300-500ms; adotamos 0,5s como valor redondo. Ressalva: não há visibilidade, a partir do firmware, de possível constante de tempo de um front-end analógico (fotodiodo + amplificador + filtro RC) entre o sensor e o conversor A/D — isso só um exame do esquema elétrico ou medição com osciloscópio pode confirmar.

---

## 2. Reinício do looping de captura de dados

**Onde está hoje:** `main/ampoule_test.cpp:24,25` — `TASK_TIME` (28000ms) + `TASK_TIME_DELAY` (2000ms) = **30 segundos reais por ciclo** (não 20s, como o cliente relatou — conferido no código e não há indício, no histórico do git deste repositório, de que o valor tenha sido diferente). Usado em `ampoules_test_timer_task()` (linha ~766):

```cpp
int64_t delay = (TASK_TIME + TASK_TIME_DELAY) - round(tt);
vTaskDelay(pdMS_TO_TICKS(delay));
```

Onde `tt` é o tempo total gasto processando **todas** as cavidades ativas dentro do mesmo `for` (medido entre `t1` antes do loop e `t2` depois — soma, não é por cavidade).

**Mudança:** vira `loop_cycle_time`, configurável, faixa **2 a 50 segundos**.

**RISCO CRÍTICO identificado e que precisa de mitigação obrigatória (não opcional):**
`delay` é calculado como `loop_cycle_time - tt`. Se `tt` (tempo real de leitura de todas as cavidades ativas) for maior que `loop_cycle_time` configurado, `delay` fica **negativo**. Esse valor é passado para `pdMS_TO_TICKS()` e depois `vTaskDelay()`, que espera um tipo **sem sinal** (`TickType_t`). Um valor negativo convertido para um tipo sem sinal vira um número enorme — na prática, a tarefa (que roda em prioridade `configMAX_PRIORITIES - 1`, quase a mais alta do sistema) trava por um tempo indeterminado ou deixa de ceder CPU corretamente, podendo starvar outras tarefas críticas (controle de aquecimento, teclado, buzzer).

**Mitigação (fórmula de validação, a ser aplicada na web E como clamp defensivo no firmware):**

```
mínimo_loop_cycle_time = N_cavidades_ativas × (led_capture_time + 0,5s)
```

Onde `N_cavidades_ativas` vem do item 4 (não do número de cavidades fisicamente presentes no momento — precisa ser o número de cavidades *habilitadas*, porque a presença física pode mudar depois de a configuração ser salva). Margem de 0,5s por cavidade cobre o overhead próprio do conversor A/D (ver seção sensor abaixo).

**Exemplo de referência usado na conversa:** ciclo=20s, captura=1s, 4 cavidades ativas → tempo de captura total = 4s, sobra 16s distribuído como 4s de intervalo entre cada cavidade; mínimo permitido nessa configuração = 4×(1+0,5) = 6s.

**Nota sobre o sensor (`light_sensor.cpp`):** o conversor A/D (CS5532, configurado em `WR1` ~50-60Hz) tem overhead próprio de leitura de ~150-200ms por canal (`read_channel_value()`: 50ms fixo + 5 iterações de ~10-20ms cada, necessárias para "esvaziar" o filtro digital sigma-delta depois de trocar de canal). Esse valor já está embutido em cada leitura, e é a base técnica da margem de 0,5s/cavidade.

---

## 3. Quantidade de amostras para cálculo do resultado (N iniciais / M finais)

**Onde está hoje:** `components/ampoule_sensor/AmpouleSensor.h`:
- `add_sensor_value()` (linhas ~388-404): buffer (`LinkedList<long> samples`) limitado a **10 valores fixos**. Quando ultrapassa 10, remove sempre o **índice 5** (hardcoded) antes de adicionar o novo valor — pressupõe implicitamente um esquema de 5 "iniciais" fixas + 5 "finais" em janela deslizante.
- `get_test_result(float average, bool)` (linhas ~506-566): usa `samples.get(size-1)` até `samples.get(size-5)` (últimas 5) e `samples.get(0)` até `samples.get(4)` (primeiras 5), soma cada grupo (`medH`, `medL`) e calcula `diff = (medH - medL) / medL * 100`, comparado contra `positive_percentage` (já configurável hoje, default 40%).
- **Único guard existente:** `if (size <= 0) return false;` — **não existe checagem de `size < 5`**. Chamado hoje sempre com >=5 amostras coletadas por coincidência dos valores fixos atuais (30s de loop × 14+ iterações antes da checagem aos 420s).

**Duplicação no front-end:** `components/httpd_app/www/pages/ampoules.html`, função `get_percentual_variation()` (linhas ~406-446) — reimplementa exatamente a mesma conta 5+5 em JavaScript, só para exibição na tela de monitoramento (`/admin`, "Análise"). Não há um terceiro lugar com essa lógica (`printer.cpp` só imprime o resultado já decidido, não recalcula nada).

**Mudança:**
- `AmpouleSensor::get_test_result()`: generalizar os acessos fixos (`size-1`..`size-5`, `0`..`4`) para laços sobre `N_iniciais` e `M_finais` configuráveis (cada um, faixa 3 a 10).
- `add_sensor_value()`: capacidade do buffer passa a ser `N + M` (não mais 10 fixo); lógica de descarte generalizada para sempre preservar as primeiras `N` e manter só as `M` mais recentes na janela final (hoje descarta sempre índice 5 fixo).
- **Guard de segurança obrigatório (independente de qualquer validação na web):** adicionar `if (size < N + M) return false;` logo no início de `get_test_result()`. Motivo: depende do item 5 (tempo de checagem antecipada) e do item 2 (intervalo do loop) — se o operador configurar uma combinação onde o tempo de checagem antecipada é curto demais para já ter `N+M` amostras coletadas, o acesso a `samples.get(índice_negativo)` é comportamento indefinido (risco de crash/reset). Esse guard é barato (2 linhas) e protege contra qualquer combinação futura, mesmo que a validação da web tenha uma falha.
- **`ampoules.html`:** atualizar `get_percentual_variation()` para ler `N`/`M` configurados via API, em vez de hardcoded 5+5 — senão a tela mostra ao operador um resultado diferente do que o firmware realmente usou para decidir positivo/negativo (risco de qualidade/confiança do operador no equipamento).

**Validação cruzada obrigatória:** `tempo_checagem_antecipada (item 5) >= (N + M) × loop_cycle_time (item 2)`, recalculada a cada mudança em qualquer um dos três.

**Ressalva de validação de método:** mudar N/M muda a característica estatística da decisão positivo/negativo. Recomendado que, antes de operar em produção fora do padrão atual (5+5), isso seja validado por quem responde pela validação do método/registro do equipamento — não é uma mudança "livre de risco" do ponto de vista de resultado clínico/laboratorial.

---

## 4. Desativar cavidades individualmente

**Estado atual confirmado:** não existe hoje nenhuma tela web para isso (verificado com o cliente — item novo, não uma configuração de algo já existente).

**Infraestrutura já existente e reaproveitável:** `AmpouleSensor::is_disabled` (`set_disabled_status()`/`get_disabled_status()`) já existe e já é respeitado em `ampoule_set_status(bool,bool,bool,bool)` (`main/ampoule_test.cpp:969-1017`):

```cpp
if (!amp1_is_disabbled) {
    ampoules[0].is_present = ampoule1;
    ampoules[0].set_apoule_alarm(ampoule1);
}
```

Ou seja: se `is_disabled == true`, a presença física da ampola nunca é atualizada para essa cavidade — ela fica congelada. E o loop principal (`ampoules_test_timer_task`) já ignora cavidades com `is_present == false` (a função `ampoule_test()` só faz algo `if (ampoules[index].is_present)`). **Conclusão: o comportamento pedido pelo cliente (readaptar o loop pra só rodar a cavidade ativa) já acontece naturalmente reaproveitando esse campo — não precisa reescrever a lógica do loop.**

**Mudança necessária:**
- Nova área na web (`restrict.html` ou página dedicada) com 4 checkboxes, uma por cavidade.
- Novo endpoint para persistir esse estado (via o blob único da seção 6, não como chamada NVS isolada).
- `is_disabled` passa a ser controlado também manualmente (hoje só é lido, nunca setado por código ativo — ver ponto de atenção abaixo).

**Ponto de atenção / risco de conflito futuro:** existe uma rotina de autodiagnóstico, `ampoule_test_check_cavity()` (`ampoule_test.cpp:504-579`), que compara leitura com LED apagado vs. aceso para detectar cavidade com defeito e chamaria `set_disabled_status()` automaticamente. **Essa rotina está hoje desativada** (chamada comentada em `main/ampoules.cpp:221`: `//ampoule_test_check_cavity();`) — por isso não há conflito agora. Mas se essa rotina automática for reativada no futuro, ela vai usar o **mesmo campo** que o toggle manual do operador, podendo reabilitar (ou desabilitar) uma cavidade contra a vontade explícita do operador. Recomendação: se/quando reativarem esse autodiagnóstico, separar em dois campos (`disabled_by_diagnostic` vs `disabled_by_operator`), combinados via OR nos pontos de checagem.

**Faixa:** 1 a 4 cavidades ativas simultaneamente (todas desabilitadas não é um estado válido — ao menos 1 deve permanecer ativa, ou o equipamento não teria função; confirmar com o cliente se esse bloqueio faz sentido antes de implementar).

---

## 5. Tempo mínimo para checagem antecipada de resultado positivo

**Onde está hoje:** `main/ampoule_test.cpp:633` — valor fixo `420` (7 minutos, em segundos), dentro de `ampoule_test()`:

```cpp
if (ampoules[index].current_time >= 420 && !ampoules[index].test_done
        && ampoules[index].is_testing) {
    bool is_positived = ampoules[index].calcule_test_result();
    if (is_positived) {
        set_date_time(index, false);
        finalize_ampoule_test(index, ampoule, true);
    }
}
```

**Comportamento real (confirmado por análise, importante deixar registrado):** essa checagem só **antecipa** o fim do teste quando o resultado já é **positivo**. Se continuar negativo, o teste segue até o fim do ciclo total (hoje fixo em 20 minutos — `DEFAULT_TIME_TEST`, `ampoule_test.cpp:27`) e imprime normalmente lá. **Não existe, no código atual, nenhuma regra de "não imprime se o ciclo for menor que 7 minutos"** — essa foi uma hipótese levantada durante a conversa e descartada após inspeção completa (não há outra ocorrência de "420" ou lógica equivalente em `printer.cpp` ou em qualquer outro ponto de impressão). Também vale registrar: hoje `ampoule_set_time_test()` (`ampoule_test.cpp:1076-1088`) ignora o parâmetro `level` recebido e sempre força 20 minutos — ou seja, hoje não existe nem como ter um ciclo de teste configurado abaixo de 7 minutos por essa via.

**Mudança:** o valor `420` vira uma variável configurável, faixa **3 a 15 minutos** (180 a 900 segundos).

**Validação cruzada obrigatória:** ver seção 3 — `tempo_checagem_antecipada >= (N + M) × loop_cycle_time`, e o guard defensivo `if (size < N+M) return false;` em `get_test_result()` cobre o caso de alguém burlar a validação da web.

**Ressalva de validação biológica:** esse tempo provavelmente reflete quanto tempo a reação do indicador biológico leva para ficar confiável. Não é uma constante puramente eletrônica — recomendado confirmar com quem validou o método antes de operar em produção com valores próximos do mínimo (3 minutos). Essa parte é responsabilidade do cliente (reagente/indicador biológico), não da engenharia de firmware.

---

## 6. Persistência e proteção contra falha de energia

**Motivação:** este projeto já teve um incidente real de configuração corrompida por queda de energia durante uma gravação (registrado em memória do projeto, sessão anterior). O padrão atual de gravação (`nvs_helpers.cpp`: `setFloat`/`setInt` com `nvs_commit` individual por chamada) grava campos relacionados em transações separadas — um corte de energia entre duas gravações pode deixar campos interdependentes (ex: `led_capture_time` e `loop_cycle_time`) em um estado inconsistente mesmo que a web tenha enviado uma combinação válida.

**Design acordado:**

1. **Escrita atômica:** os 5 campos (mais o estado das 4 cavidades do item 4) são agrupados em uma única struct e gravados com uma única chamada `nvs_set_blob` + `nvs_commit` — nunca como 5+ chamadas separadas. Isso já resolve o problema de "um campo grava, outro não" usando a garantia nativa do ESP-IDF (um commit é atômico).

2. **Redundância contra corrupção do próprio blob:** o mesmo blob é gravado em **duas chaves NVS diferentes** ("Área A" e "Área B"), cada uma com um CRC calculado sobre o conteúdo. Ao ler (no boot), calcula-se o CRC de cada área e compara-se uma com a outra.

3. **Comportamento em caso de divergência (decidido explicitamente, sem "vencedor"):**
   - Nenhuma das duas áreas é considerada automaticamente correta.
   - O firmware aciona o alarme sonoro (`buzzer_alarm()`, já existe em `main/buzzer.cpp`) e passa a operar **imediatamente** com os valores padrão de fábrica em memória (RAM) — o equipamento continua funcionando, não trava, mas com configuração conhecida e segura.
   - Ao entrar na área protegida da web, uma mensagem de erro é exibida com um botão dedicado "Restaurar padrão de fábrica" — que grava os valores padrão de volta nas duas áreas (A e B), encerrando o alarme permanentemente.
   - Esse botão é **novo e separado** do botão "Resetar dispositivo" já existente (`reset.html`, `/api/v1/reset`, `reset_user_data()` em `nvs_utils.cpp:433`) — aquele já reseta um escopo totalmente diferente (instituição, histórico de 12 testes, dados temporários) e **não será alterado**.

4. **Alarme:** contínuo/intermitente até resolução (mesmo padrão dos alarmes de temperatura já existentes no equipamento), não um beep único — mais fácil do operador perceber.

**Pontos em aberto para quando a implementação começar:**
- Definir se esse mesmo mecanismo (blob duplo + CRC) deve ser estendido para `positive_percentage` e `calib_factor`, hoje tão ou mais críticos que os campos novos e sem nenhuma proteção. Foi levantado na conversa mas não decidido.
- Definir exatamente o algoritmo de CRC a usar (ex: `esp_rom_crc32`, já disponível no ESP-IDF — o projeto já usa CRC8 em `components/onewire`/`components/ds18x20`, mas para outro protocolo, não reaproveitável diretamente).

---

## 7. Resumo das faixas finais

| Item | Descrição | Faixa |
|---|---|---|
| 1 | Tempo de captura (LED aceso) | 0,5 a 7 segundos |
| 2 | Tempo de reinício do looping | 2 a 50 segundos (mínimo variável, ver fórmula seção 2) |
| 3 | Amostras iniciais / finais (cada uma) | 3 a 10 |
| 4 | Cavidades ativas | 1 a 4 |
| 5 | Tempo mínimo p/ checagem antecipada | 3 a 15 minutos |

Todas as validações cruzadas (itens 1↔2, 2↔4, 3↔5) devem ser recalculadas a cada alteração de qualquer campo relacionado, tanto na tela (auto-ajuste para o mínimo válido) quanto como guard defensivo no firmware (ver seção 3).

---

## 8. Arquivos que serão tocados na implementação (mapa de referência)

- `main/ampoule_test.cpp` — separar `TASK_TIME_DELAY` em `led_capture_time`/`loop_cycle_time`; tornar `420` (early-check) configurável; usar N/M configuráveis.
- `components/ampoule_sensor/AmpouleSensor.h` — generalizar `add_sensor_value()` e `get_test_result()` para N/M configuráveis; adicionar guard `size < N+M`.
- `components/httpd_app/www/pages/ampoules.html` — atualizar `get_percentual_variation()` (JS) para não hardcoded 5+5.
- `components/httpd_app/www/pages/restrict.html` (ou nova página) — novos campos de configuração + 4 checkboxes de cavidade + botão "Restaurar padrão de fábrica".
- `main/app_httpd.cpp` — novos endpoints (GET/POST) para os 5 campos + estado das cavidades + endpoint de restaurar padrão.
- `main/nvs_utils.cpp` + `components/nvs_helpers` — nova função de gravação em blob único, duplicado em duas áreas, com CRC.
- `main/buzzer.cpp` — reaproveitar `buzzer_alarm()` para o alarme de divergência de configuração.
- `main/ampoule_test.cpp` (`ampoule_set_status`) — nenhuma mudança estrutural necessária para o item 4 (infraestrutura já existe via `is_disabled`), só passar a permitir setar esse campo a partir da web.
