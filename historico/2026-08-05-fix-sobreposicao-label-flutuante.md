# Fix — texto sobreposto nos campos da tela web (label flutuante do Bootstrap)

**Data:** 05/08/2026
**Branch:** `feature/config-web`
**Motivo:** cliente ligou reportando problema na tela do webserver e mandou 11 prints do WhatsApp (celular, `192.168.10.10`) com marcações em verde. Nas telas de **Configurações Avançadas > Parâmetros de Teste** e **Configurações > Dispositivo**, o valor digitado nos campos numéricos aparecia sobreposto ao texto do label — ex.: "graus" virou "6Qus", "segundos" virou "0,5undos", "teste" virou "53te", "resultado positivo" virou "8sultado", "reconhecido" virou "1br reconhecido".

## Causa raiz

Todos os campos afetados usam o padrão `.form-floating` do Bootstrap (label que "flutua" pra cima e encolhe quando o campo tem valor). Esse padrão do Bootstrap assume label de **uma linha só** — a transformação CSS (`scale(.85) translateY(-.5rem)`) desloca o bloco inteiro do label, mas não recalcula altura pra texto de 2 linhas. Como os labels aqui são frases longas em português, muitas com parênteses (`"... (0,5 a 7 segundos)"`, `"... (3 a 10)"`), o texto quebra em 2 linhas na largura de tela de celular, e a 2ª linha do label cai visualmente em cima do valor do input.

Não é um bug de dado/cálculo — os valores salvos e a lógica de `recalculateMinimums()`/`recalculateTemperatureHints()` (`advanced_config.html`) sempre estiveram corretos. É puramente CSS/layout.

## Fix

Convertido de label flutuante (`form-floating`, label depois do input) pra label fixo acima do input (`form-label`, label antes do input) — não precisa de CSS customizado nem de calcular altura por número de linhas, e não muda nenhum `id` nem atributo `key` de tradução (JS/i18n intocados).

**1ª leva** (campos que o cliente marcou em verde nos prints): `led_capture_time`, `early_check_time`, `heater_setpoint`, `heater_min_temp`, `heater_max_temp`, `heater_release_temp` (`advanced_config.html`) + `buzzer_alert_timeout_min` (`settings.html`).

**2ª leva** (usuário reportou que `loop_cycle_time` e as duas `samples_*` continuavam com o mesmo bug depois do 1º flash — a suposição inicial de que labels mais curtos não quebravam linha estava errada na tela real do celular): `loop_cycle_time`, `samples_initial`, `samples_final` (`advanced_config.html`).

**3ª leva** (preventivo, mesmo padrão em todo o resto do webserver pra não haver mais nenhum campo com esse risco): `print_count_label` (`history.html`), `positive_percentage` (`restrict.html`), `device_date_time`/`date_time`/`institution` (`settings.html`), `num_serie_atual`/`num_serie` (`serialnumber.html`), `password1` (`reset.html`), `temperature_thermometer` (`calibration.html`).

De quebra, corrigido `for="num_serie"` → `for="num_serie_atual"` no label de "Número de Série (Atual)" em `serialnumber.html` (apontava pro id errado, herdado do copy-paste do campo "Novo"; sem efeito visível porque o campo é `disabled`, mas ficou correto).

## Verificação antes do flash

Sem hardware disponível pra testar na hora, subi um servidor Python local (`preview_server.py`, fora do repo) reproduzindo o roteamento de URL do firmware (`/admin/advanced_config`, `/bootstrap/...`, etc., ver `main/app_httpd.cpp`), abri no Chrome com a janela em 390×844 (tamanho de celular) e digitei os mesmos valores dos prints do cliente (0.5, 60, 53, 68) — confirmado visualmente que o label fica limpo acima do campo e o valor aparece inteiro, sem sobreposição, replicando exatamente o cenário do bug reportado.

## Testado no equipamento (COM20, ESP32-S3 `dc:da:0c:49:07:a8`)

- Build limpo via Docker `espressif/idf:release-v5.1` (fullclean + reconfigure + build) — 2x, uma pra cada leva de fix.
- `.gz` de cada `.html` alterado regenerado manualmente (`gzip -kf9`) antes de cada build, conforme convenção do projeto — checado com o script de staleness, nenhum arquivo desatualizado.
- Gravado com sucesso nas duas rodadas (`esptool`, hash verificado nas 3 partições, reset via RTS).
- **1ª leva confirmada fisicamente pelo cliente** (campo "Tempo de LED" sem sobreposição).
- **2ª e 3ª leva: aguardando confirmação física do cliente** — só aplicadas e gravadas, ainda não validadas no uso real dele.

## Pendente pra próxima sessão

- Aguardar retorno do cliente confirmando que `loop_cycle_time`, `samples_initial`, `samples_final` e os campos das outras telas (Histórico, Área Restrita, Reset, Calibração, Configurações) não apresentam mais sobreposição.
- Se o cliente reportar mais algum campo com o mesmo sintoma, verificar se sobrou algum `form-floating` com label+input diretamente aninhados (rodar `grep -A2 "form-floating" components/httpd_app/www/pages/*.html` — os que restaram são só wrappers externos inertes, sem label/input direto dentro).
