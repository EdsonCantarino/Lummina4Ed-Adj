# Checkpoint 12/08 (tarde) — fix reboot da impressora + UI web + ferramenta de log

**Data:** 12/08/2026
**Branch:** `feature/config-web`
**Commits:** `2696ab1` (fix reboot), `a64bc31` (UI web) — ambos pushed.

## 1. Tela web — campo "Data/Hora Servidor" + renomeações de menu

- `settings.html`: campo "Nova Data e Hora" virou **"Data/Hora Servidor"**, agora
  somente-leitura. Carrega com a hora do navegador (`moment()`) ao abrir a
  página (antes espelhava o RTC), e busca a hora do navegador **de novo** no
  momento de clicar "Salvar" — evita gravar hora desatualizada se a página
  ficou aberta um tempo. Removido o botão "Atualizar hora do Lummina?"
  (ficou redundante) e a lib do datepicker (não usada mais).
- Menu "Configurações": "Dispositivo" → **"Data/Hora Buzzer"**, "Calibração"
  → **"Calib. Temperatura"**, "Resetar dispositivo" → **"Padrão Fábrica"**
  (título e item de menu), botão de confirmação da tela de reset → **"Assume
  padrão de Fábrica"**. Traduções en-us/es-es atualizadas junto. `.gz`
  regenerado em todas as páginas/idiomas tocados.
- **Validado fisicamente via Chrome** (PC já conectado na rede `MAX-C3B828`,
  IP `192.168.10.10`): campo carrega com hora do navegador, botão Salvar
  abre modal com hora "atual do equipamento" vs "nova hora" (buscada de
  novo, diferente da carregada no load), Confirmar grava — RTC do
  equipamento confirmado atualizado corretamente após reload da página.
  Login web usa **`L4XXBIO`/`128500A`** (hardcoded em `security.cpp`,
  `getUserMaster()`/`getPasswordMaster()` — os valores de
  `CONFIG_DEFAULT_BASIC_AUTH_*` no `sdkconfig` são mortos, não usados em
  lugar nenhum do código real de autenticação). Senha do Wi-Fi (SSID
  `MAX-<ultimos 3 bytes do MAC>`) também não é fixa — é calculada de
  `getPassword()`/`security.cpp` a partir do número de série; para o
  serial default `002300P4` dá `56275627`.

## 2. Bug do reboot da impressora — investigação e fix

**Sintoma reportado pelo usuário:** removeu uma ampola com a impressora
desconectada (cabo tirado no meio de um teste anterior que tinha
funcionado); esperado reiniciar automaticamente ~10s depois e resolver o
ticket pendente no boot seguinte — não reiniciou.

**Causa raiz:** `attempt_safe_printer_recovery()` (`printer.cpp`) tinha um
guard `if (!is_printer_connected())` que bloqueava o reinício sempre que a
impressora **não estava conectada no momento exato da checagem** — sem
distinguir "nunca teve impressora nesse boot" (motivo original do guard,
bring-up ADS1248 em bancada, 07/08 — reiniciar não ajuda em nada) de
"impressora esteve conectada e imprimiu nesse boot, só foi desconectada no
meio de um teste depois" (reiniciar é a única recuperação conhecida — caso
relatado pelo cliente 05/08). O guard tratava os dois casos igual, porque só
olhava o estado atual da conexão.

**Duas tentativas erradas antes de acertar** (revertidas, não ficaram no
código final):
1. Remover o guard inteiramente → reiniciaria a cada teste sem impressora,
   inclusive em bancadas que nunca vão ter impressora. Usuário apontou que
   não era isso ("vamos a sequência correta..").
2. Flag "impressora já conectou alguma vez desde o boot" (latch que nunca
   desliga) → usuário apontou risco de uma conexão acidental/breve grudar a
   flag em true pro resto da sessão mesmo se a impressora sumir de vez
   depois.

**Fix final (aceito pelo usuário, com a limitação reconhecida):** foto
única (`printer_present_at_boot`) tirada uma vez, ~10s após o boot, dentro
de `resolve_boot_pending_history_task()` (mesmo timing que já esperava a
impressora enumerar via USB). `attempt_safe_printer_recovery()` passou a
checar essa foto em vez do estado atual. Limitação aceita explicitamente: se
a impressora só for conectada **depois** dessa janela de ~10s inicial, o
reinício de recuperação não vale pra ela nessa sessão ("se esqueceram vai
ter um reboot e segue a vida" — não considerado problema).

**Validado fisicamente, sequência completa (COM22):**
1. Boot com impressora conectada → log confirma `"Boot: impressora presente
   no boot"`.
2. Teste de ampola rodando → cabo da impressora retirado no meio (`USB
   DEV_GONE` real, log `"Deregistering Client"`) → teste termina → falha ao
   imprimir (`is_printer_error`) → ticket marcado pendente.
3. Ampola removida → conta carência de 10s (`"seguro pra reiniciar mas
   aguardando carencia (N/10000 ms sem ampola)"`) → **reinicia sozinho**
   (`RESTART_ID=1`, `rst:0x3 RTC_SW_SYS_RST` — reset por software, não queda
   de energia).
4. Boot seguinte (impressora ainda ausente) → log `"Boot: impressora
   ausente no boot"` + `"marcando ticket id_test=... como impresso sem
   confirmacao de impressao"` — ticket resolvido sem tentar imprimir.
5. Próximo boot (impressora reconectada) → `"Boot: nenhum ticket
   pendente"` — **não reimprime nada**, confirmando que o ciclo não repete.

Logs brutos em `historico/Testes/monitor_com22_*_12_08.log` (vários arquivos,
cobrindo a investigação inteira incluindo os dois testes que reproduziram o
bug antes do fix).

## 3. Reset de fábrica ("Padrão Fábrica") testado via web

Testado ponta a ponta pelo Chrome: botão → modal pede senha → senha
`56275627` (a mesma do Wi-Fi, `getPassword()` — não é a senha master de
login) aceita → toast de sucesso → `esp_restart()` automático por software
(`RESTART_ID=3`) → histórico confirmado vazio (`0/0`, "Nenhum resultado
salvo") depois do reboot. Bloqueio por ampola presente (`ampoule_any()`)
confirmado por leitura de código, não testado fisicamente com ampola
inserida.

## 4. Ferramenta nova: `tools/serial_logger.py` / `serial_logger.exe`

Cliente vai testar a unidade ADS1248 com autoclave real e mandar o log pra
análise. Criado um logger standalone (PyInstaller, `--onefile`) pra
facilitar: `serial_logger.exe COM7` (ou sem argumento, lista portas e pede
escolha) grava tudo da serial num `.log` com timestamp real por linha,
remove códigos ANSI, e **reconecta sozinho** se a porta cair (reboot ou
queda de energia do equipamento durante o teste longo). Testado
manualmente contra a COM22, confirma captura e reconexão.

`.bin` enviado ao cliente: `build/Lummina4_MAXXIMED_v2026_08_12.bin`,
sdkconfig atual = **ADS1248 + `CONFIG_ADC_DEBUG_SERIAL=y`** (não CS5534) —
confirmado com o usuário que esse cliente específico tem hardware ADS1248,
então bate. O debug serial ligado é proposital: dá as linhas `[ADCDBG]
chip=ADS1248 ch=... raw=...` que ajudam a analisar o comportamento do AD no
log que ele mandar de volta.

## Pendências (não testadas ainda)

- Reimpressão manual **física** (botão do equipamento, `keyboard.cpp` →
  `PRINT_NOTIFY_BUTTON`) — não existe reimpressão manual pela tela web,
  só automática (fim de teste, boot, reconexão) ou pelo botão físico.
- Mais de 1 ticket pendente ao mesmo tempo (`PENDING_REPRINT_MAX = 4`) —
  só foi exercitado 1 ticket por vez hoje.
- LED que o usuário lembrava de piscar pedindo pra retirar a ampola no fim
  do teste — não encontrado no código (`ampoules_leds_positived` acende
  fixo, não pisca), não investigado mais a fundo, ficou em aberto.
- Ideia discutida e **descartada** (não implementada): reimpressão por
  número de ticket digitado na tela de histórico. Alternativa mais simples
  ventilada (botão de reimprimir por linha da tabela) também não
  implementada.
