# Análise 19/08 — linha do tempo do ciclo de leitura e mecanismo real do fix de 18/08

**Data:** 19/08/2026
**Branch:** `feature/config-web`
**Commit de referência:** `dfe68c4` (fix analisado, sem mudança de código nesta sessão)
**Status:** só análise/discussão, nenhum código alterado.

Continuação do checkpoint
`2026-08-18-fix-reinicio-falso-temperatura-aquecedor-4-cavidades.md`. Objetivo
desta sessão: entender em detalhe *como* o fix de 18/08 ganha tempo de
aquecedor-ligado, a partir dos dois logs de validação parcial (16:36–16:54)
e do diff exato do commit.

## Recuperação do histórico de logs

Dois logs novos identificados em Downloads, próximos de 16:54 do dia 18/08:

- `monitor_COM4_2026-08-18_163624.log.txt` (16:36:25→16:44:54, ~8min29s)
- `monitor_COM4_2026-08-18_164725.log.txt` (16:47:25→16:54:09, ~6min44s)

Achados já registrados no checkpoint de 18/08 (seção "Validação física
parcial"): nenhum reinício falso reproduzido, temperatura nunca saiu do
range, log 2 mostrou recuperação de 54,5°C→55,0°C no fim. Detalhe novo
encontrado aqui: os dois logs usam ranges de alarme diferentes (`> 53.0 e
< 67.0` no log 1, `> 53.0 e < 68.0` no log 2) — consistente com ajustes de
`advanced_config` feitos durante a investigação do dia anterior.

## O fix não muda o tempo de leitura, muda a proporção livre/desligado

`Tempo total de execucao` (tempo real pra ler as 4 cavidades) é uma métrica
de hardware/driver (SPI/ADC), não afetada pelo fix de controle do
aquecedor:

- Log 1: `2907 ms` por rodada (a unidade impressa no código diz "us" mas o
  valor é ms — bug de rótulo já conhecido, não corrigido).
- Log 2: `4507 ms` — **idêntico** ao valor do log de campo original do
  cliente (mesma condição de estouro que causou o bug).

## Releitura do diff real do fix (`git diff dfe68c4^ dfe68c4 -- main/ampoule_test.cpp`)

Ao conferir o código antigo de verdade (não só a descrição do checkpoint),
duas correções à narrativa de 18/08:

1. **`prepare_test()` antigo já deixava o aquecedor livre durante toda a
   janela de captura do LED** (`vTaskDelay(capture_ms)` vem *antes* de
   `set_heater_controlling(false)`) — só ficava forçado desligado durante a
   leitura do ADC em si, depois da captura. A mudança do fix (desligar só
   nos últimos 100ms da captura) não reduz esse tempo desligado — na
   prática **aumenta em até 100ms por cavidade** (o desligamento começa
   100ms mais cedo do que antes). Item cosmético/de ruído elétrico, não de
   economia de tempo.
2. **O ganho real inteiro está no piso de folga entre rodadas**
   (`ampoules_test_timer_task`), que subiu de 100ms pra 2000ms
   (`HEATER_RECOVERY_MIN_MS`). No cenário do log 2 (`tt=4507ms >
   cycle_ms=4000ms`), o delay calculado é negativo e cai direto no piso —
   antes 100ms, agora 2000ms. **Ganho real: 1900ms por rodada**, ~475-500ms
   por cavidade quando dividido por 4 — bate com o "ganhamos 500ms" citado
   na conversa.

## Linha do tempo medida (log 2, cenário que reproduz a falha original)

Duração por cavidade medida via prints `Numero Sequencial→Valor recebido`:

| Cavidade | Duração total | Livre (captura) | Forçado OFF (100ms + leitura ADC) |
|---|---|---|---|
| 1 | 1126ms | 400ms | 726ms |
| 2 | 1180ms | 400ms | 780ms |
| 3 | 1169ms | 400ms | 769ms |
| 4 | 1106ms | 400ms | 706ms |

```
t=0     ────┐ Cav.1: LED UV liga, aquecedor LIVRE
t=400   ────┤ Cav.1: aquecedor OFF (100ms cauda da captura)
t=500   ────┤ Cav.1: aquecedor OFF (leitura ADC, ~626-680ms)
t=1145  ────┤ Cav.1: aquecedor religado, LED UV desliga
            │
t=1145  ────┤ Cav.2: LIVRE
t=1545  ────┤ Cav.2: OFF (cauda)
t=1645  ────┤ Cav.2: OFF (leitura)
t=2290  ────┤ Cav.2: religa
            │
t=2290  ────┤ Cav.3: LIVRE
t=2690  ────┤ Cav.3: OFF (cauda)
t=2790  ────┤ Cav.3: OFF (leitura)
t=3435  ────┤ Cav.3: religa
            │
t=3435  ────┤ Cav.4: LIVRE
t=3835  ────┤ Cav.4: OFF (cauda)
t=3935  ────┤ Cav.4: OFF (leitura)
t=4580  ────┤ Cav.4: religa ← fim da rodada de leitura (≈4507-4606ms medido)
            │
t=4580  ────┤ FOLGA GARANTIDA (HEATER_RECOVERY_MIN_MS): aquecedor 100% LIVRE
t=6580  ────┘ próxima rodada começa (cavidade 1 de novo)
```

**Totais por rodada (6580ms):**
- Aquecedor OFF (forçado): 4 × ~745ms ≈ 2980ms (**45%**)
- Aquecedor LIVRE: 4 × 400ms + 2000ms de folga ≈ 3600ms (**55%**)

**Comparação com o código antigo** (mesma leitura de 4507ms, off só
durante a leitura ADC, piso de folga de 100ms): OFF ≈ 2580ms, LIVRE ≈
2100ms, rodada total 4680ms → **45% livre** (era o oposto da proporção
nova). O ganho absoluto de tempo livre por rodada é de ~1500ms líquidos
(1900ms a mais de folga entre rodadas, menos ~400ms a mais de OFF por
cavidade introduzido sem querer pelo item 1).

### Carta ligado/desligado (assumindo "livre" = fisicamente ligado)

Premissa: nesse cenário de temperatura em queda, o aquecedor não teria
tempo de bater no teto da histerese (setpoint+1°C) durante as janelas
curtas de recuperação — então "livre" foi tratado como fisicamente ligado
o tempo todo. Não há log direto do estado do GPIO pra confirmar; é uma
inferência da discussão, não uma medição.

```
Legenda: █ = ligado   ░ = desligado (forçado, durante leitura do ADC)
Escala:  1 caractere ≈ 100ms

t(ms)   0    500   1000  1500  2000  2500  3000  3500  4000  4500  5000  5500  6000  6500
        |    |     |     |     |     |     |     |     |     |     |     |     |     |
        ████░░░░░░░████░░░░░░░░████░░░░░░░████░░░░░░░░████████████████████████████████
        └─C1─┘└─C1 OFF─┘└C2┘└C2 OFF──┘└C3┘└─C3 OFF─┘└C4┘└─C4 OFF──┘└────FOLGA 2000ms────┘
```

## Ideias discutidas e descartadas/adiadas

- **Subir o setpoint em +1°C durante o teste** (dar mais banda de histerese
  pra cima, criando margem antes do piso de alarme): descartada nesta
  análise porque só funcionaria se o aquecedor chegasse a *desligar* nas
  janelas livres entre leituras (bater no teto da histerese) — pelos dados,
  ele provavelmente fica ligado o tempo todo nessas janelas curtas
  (nunca alcança nem o teto atual), então mudar o alvo não teria efeito.
  Além disso, setpoint não é só parâmetro de controle: é a temperatura de
  incubação real do ensaio — mudar isso, mesmo temporariamente, é decisão
  de especificação clínica/física, não só de engenharia de controle (ver
  `project_hardware_60C_origin` — equipamento validado a 60°C por 1 ano).
- **Aumentar a potência da resistência** (hardware): única alternativa
  restante *se* o aquecedor de fato já estiver saturado (ligado 100% do
  tempo livre disponível) — precisa confirmar essa premissa antes de
  considerar, e é mudança de hardware, mais lenta/cara que firmware.
- **Melhorar isolamento térmico do bloco**: ataca o mesmo problema pelo
  lado da perda de calor em vez da geração — mais barato/seguro que mexer
  em potência elétrica, mas precisa avaliar junto de quem projetou o bloco
  (risco de desuniformidade entre cavidades, e de reter calor demais em
  caso de falha real do controle).

## Pendente

Nenhuma dessas ideias foi implementada — sessão só de análise/discussão.
Continua pendente o mesmo item do checkpoint de 18/08: validar o fix atual
(`dfe68c4`) num teste longo completo (~1h, replicando o cenário do
cliente) antes de considerar suficiente ou partir pra alternativas de
hardware/isolamento.
