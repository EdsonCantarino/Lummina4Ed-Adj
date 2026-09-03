# 03/09 — Modo PA20 + branding multimarca (MedControl/TechSteri/Baumer) + conversor AD por linha de comando

**Data:** 03/09/2026
**Branch:** `feature/config-web`
**Status:** implementado, compilado e **validado fisicamente na bancada**
(COM20, AP `MAX-4907A8`). Todas as 5 marcas (MAXXIMED, BIOSTERONS,
MEDCONTROL, TECHSTERI, BAUMER) compilam; 3 delas foram gravadas e
confirmadas no equipamento real nesta sessão.

## 0. Confirmação de campo do fix de 28/08

Sessão começou com o retorno do cliente sobre o fix de troca antecipada
de canal do ADS1248 (commit `e9fb523`) — testado em mais unidades de
campo, **tudo ok**. Ver atualização em
[[2026-08-28-fix-troca-antecipada-canal-ads1248-ruido-adc]].

## 1. Modo de operação PA20 (quarto modo, além de Normal/ETO/CRC1)

Pedido do cliente, com imagem de referência (tabela de parâmetros):
LED 0,5s, looping 12s, amostras 5+5, cavidades todas, checagem
antecipada 7min, setpoint 60°C, mínima 53°C, máxima 67°C, liberação
55°C. Esclarecido por conversa (não só a imagem) que:
- Comportamento de botão físico = igual ao **Normal** (selecionável,
  beep, LED) — diferente do CRC1 (que é pra 1 cavidade só, PA20 é o
  oposto: força as **4** cavidades habilitadas).
- Tempo dos botões físicos = tabela **legada** "de versões anteriores":
  20min → 1h → 2h → 3h (recuperada do código comentado do commit
  inicial do projeto, `7836c58`), diferente da tabela atual do
  Normal/CRC1 (5min → 20min → 1h → 3h, sem o 2h).
- Todos os 9 valores da tabela (não só cavidades) ficam **forçados**
  pelo backend ao selecionar PA20, sobrescrevendo o que a tela web
  mandar — mesmo padrão que o CRC1 já usa pra forçar a cavidade única.

Implementação:
- `advanced_config.h`: `OPERATION_MODE_PA20 = 3` no enum.
- `app_httpd.cpp` (POST `/api/v1/advanced_config`): bloco de forçagem
  do perfil completo quando `operationMode == "pa20"` (cavidades +
  7 campos numéricos), logo após o bloco equivalente do CRC1.
- `ampoule_test.cpp` (`ampoule_set_time_test()`): branch por modo —
  PA20 usa a tabela legada, os outros mantêm a atual.
- `advanced_config.html` + 4 locales: checkbox PA20 novo, mutuamente
  exclusivo; ao selecionar, trava (desabilita) na tela todos os campos
  que o backend vai forçar (cavidades + LED + looping + amostras +
  checagem + 3 temperaturas editáveis).
- `.gz` de `advanced_config.html` e dos 4 `locales/*.json` regenerados
  manualmente (`gzip -kf9`).

### Validado fisicamente
- Tela web: PA20 selecionado, salvo, log serial confirma
  `modo=PA20 cavidades=[1,1,1,1] checagem=420s temp[setpoint=60.0
  min=53.0 liberacao=55.0 max=67.0]` — bate exato com o pedido.
- 4 slots testados, cada um num tempo diferente da tabela legada:
  cavidade 4 = 1200s (20min), cavidade 1 = 3600s (1h), cavidade 2 =
  7200s (2h), cavidade 3 = 10800s (3h) — todos batendo.
- Slot de 20min rodado até o fim: `TESTE FINALIZADO COM SUCESSO`,
  duração real 14:11:09→14:31:09 (exatos 20min), ticket impresso.

## 2. Bug real encontrado em bancada: `time_test` residual do boot

Ao inserir a ampola na cavidade 4 logo depois de salvar PA20 (sem tocar
o botão de tempo físico antes), o teste rodou com **300s (5min)** em vez
dos 1200s (20min) esperados no nível inicial do PA20.

**Causa raiz:** `ampoules[i].time_test` só é atualizado quando o botão
físico de tempo é solto (`ampoule_set_time_test()` chamado de
`button_event_task`, `keyboard.cpp`). No boot, `init_ampoules()` grava
`DEFAULT_TIME_TEST = 5*60` direto, sem consultar a tabela do modo atual
— esse valor só coincide com o nível inicial do Normal/CRC1 (que também
é 5min), não com o do PA20 (que é 20min no mesmo nível). O mesmo
problema existe (mais raro de notar) sempre que o modo é trocado pela
tela web sem reboot e o botão físico daquela cavidade não é tocado
depois — já havia um fix pontual só pro ETO pra esse tipo de caso (força
20min no início do teste), mas Normal/CRC1/PA20 não tinham nada
equivalente.

**Fix definitivo (não só o boot):** `ampoule_test()` agora re-sincroniza
`time_test` a partir da posição **real** do botão físico no início de
cada teste (`test == 1`), pra qualquer modo que não seja ETO — não só
confia no valor que já estava lá.
- `keyboard.h`/`keyboard.cpp`: novo getter
  `keyboard_get_time_level(int ampoule_id)` — só leitura do estado já
  mantido por `button_event_task`, não mexe em nenhum dos gotchas de
  escrita concorrente já documentados (`xTaskNotify`/`led_panel`).
- `ampoule_test.cpp`: no bloco que já tratava o caso especial do ETO,
  adicionado `else` chamando
  `ampoule_set_time_test(ampoule, keyboard_get_time_level(ampoule))`.

### Validado fisicamente (regressão + fix)
- Depois do fix: ampola inserida em PA20 sem tocar botão → 1200s (20min)
  corretos, confirmado no log.
- CRC1 testado depois (regressão): 300s (5min) exatos, igual sempre foi
  — o fix generalizado não quebrou o comportamento antigo do CRC1.

## 3. Branding multimarca: MedControl, TechSteri, Baumer

Cliente vende o mesmo equipamento sob 4 marcas (Maxximed, MedControl,
Técil/TechSteri, BioSterons) + Baumer. O mecanismo de troca de marca via
`-Logo` na compilação já existia (`Troca de Logo.txt`), e os 5 arquivos
de logo (`.png`/`.png.gz`) já estavam embutidos no firmware via
`EMBED_FILES` (`components/httpd_app/CMakeLists.txt`) — só faltava
cadastrar 3 delas (MedControl, TechSteri, Baumer) na tabela de
`main/branding.cpp`, que só tinha MAXXIMED e BIOSTERONS.

Textos do ticket impresso (`partner_upper`/`partner_formatted`/
`type_test`) recuperados da versão do firmware **anterior** à
refatoração pra `branding.cpp`
(`D:\Github\ECK\Maxximed\lumina4\main\printer.cpp`, `get_partner_name()`
/ `get_partner_name_formatted()` / `get_type_test()`), onde cada marca
tinha esses textos hardcoded por comparação de string:

| Marca | partner_upper | partner_formatted | type_test |
|---|---|---|---|
| MEDCONTROL | MEDCONTROL | Medcontrol | **BI - Test** (diferente do resto) |
| TECHSTERI | TECHSTERI | Techsteri | Clicktest |
| BAUMER | BAUMER | Baumer | Clicktest |

"Técil" mencionado inicialmente pelo cliente é o mesmo que TechSteri —
confirmado pelo próprio logo (`tech_steri.png`, que estampa
"TechSteri — Tecnologia em Esterilização"), provável autocorreção/erro
de digitação. Ficou pendente (não decidido) se o texto do ticket deveria
usar "TechSteri" (S maiúsculo, igual o logo) em vez de "Techsteri"
(minúsculo, igual o firmware antigo) — mantido como estava no legado por
enquanto.

### Validado fisicamente
- MedControl e TechSteri: compilados, gravados no COM20, confirmados
  visualmente (logo na tela) pelo usuário.
- Baumer: compilado e gravado, confirmação visual pendente de resposta
  do usuário no momento em que este histórico foi escrito.
- BioSterons: não testado nesta sessão (já existia antes, não é novidade
  desta sessão).

## 4. Conversor AD (ADS1248/CS5534) selecionável por linha de comando

Pedido do cliente: mesmo tratamento do `-Logo` pro conversor AD, que até
então só dava pra trocar via `idf.py menuconfig` manual (Kconfig
`choice ADC_CHIP`, `main/Kconfig.projbuild`).

**Achado antes de mexer:** o `sdkconfig` (versionado no git) estava
modificado sem commit desde antes desta sessão começar, com CS5534
selecionado — mas o HEAD commitado tinha ADS1248 (bate com o hardware
real da bancada/produção). Bate com o `historico/2026-08-28`: a réplica
da técnica no CS5534 foi testada só por simetria de código, sem
hardware, provavelmente alguém rodou `menuconfig` pra testar compilação
e não reverteu. Corrigido: `sdkconfig` e `sdkconfig.defaults` voltaram
pra ADS1248 (confirmado com o usuário antes, ver pergunta no meio da
sessão).

Implementação em `compila_Lummina4EdAdj.ps1`:
- Novo parâmetro `-Adc ADS1248|CS5534` (padrão `ADS1248`).
- Dois arquivos overlay novos, `sdkconfig.adc.ads1248` e
  `sdkconfig.adc.cs5534`, passados via
  `-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.adc.<escolha>` no
  `idf.py reconfigure` — necessário porque o `sdkconfig` já respondido
  não é sobrescrito por `sdkconfig.defaults` sozinho.
- Patch direto nas 2 linhas do `sdkconfig` antes do `fullclean`, como
  reforço pro caso de build incremental sem fullclean.

**Bug real no meio do caminho:** o patch direto por regex não funcionava
— rodava sem erro mas nunca trocava nada, porque o `sdkconfig` usa
quebra de linha `CRLF` e o regex usava `$` (fim de linha) sem contar o
`\r` sobrando antes do `\n`. Corrigido pra `[ \t]*\r?$`. Sem esse fix, 2
builds de teste saíram silenciosamente com o chip errado (confirmado
comparando `-Adc CS5534` pedido vs `sdkconfig` resultante).

### Validado
- `-Adc ADS1248` (padrão) e `-Adc CS5534` testados nos dois sentidos,
  `sdkconfig` conferido depois de cada build — troca funcionando nos 2
  sentidos depois do fix do regex.
- Build final com CS5534+MAXXIMED gerado só pra confirmar que compila
  (pedido do cliente) — **não gravado** no equipamento (hardware da
  bancada é ADS1248, gravar CS5534 nele quebra o sensor óptico). Esse
  `.bin` sobrescreveu `build\Lummina4_MAXXIMED_v2026_09_03.bin` (mesmo
  nome de arquivo pra qualquer `-Adc`/`-Logo`, ver item 5) — atenção pra
  não gravar ele por engano numa próxima sessão sem recompilar antes.

## 5. Documentação nova

`doc/compilacao-firmware.md` — explica os parâmetros `-Logo` e `-Adc` do
`compila_Lummina4EdAdj.ps1`, tabela de textos de ticket por marca, e a
inconsistência conhecida do nome do `.bin` (sempre "MAXXIMED" no nome do
arquivo, independente do `-Logo` escolhido — variável separada no
`CMakeLists.txt` raiz que não lê o mesmo `-Logo` que o `branding.cpp`
usa em runtime; não afeta o conteúdo do firmware, só atrapalha rastrear
qual `.bin` é qual depois do build).

## Pendente pra próxima sessão

- Confirmar visualmente o logo Baumer gravado (aguardando resposta do
  usuário).
- Decidir "TechSteri" vs "Techsteri" no texto do ticket (S maiúsculo ou
  não).
- Recompilar `build\` de volta pro padrão seguro (MAXXIMED + ADS1248)
  antes de qualquer gravação futura sem querer usar o build de teste
  CS5534 que ficou por último.
- Corrigir (ou documentar melhor) a inconsistência do nome do `.bin` não
  refletir o `-Logo`/`-Adc` usados.
- Testar impressão de ticket completo (não só logo na tela) das 3 marcas
  novas, conferindo os textos exatos.
