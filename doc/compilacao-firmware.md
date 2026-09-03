# Lummina 4 — Como compilar o firmware (marca/logo e conversor AD)

**Data de referência:** 03/09/2026

Este documento explica os parâmetros de linha de comando do script de
compilação — qual marca (logo + textos do ticket impresso) e qual
conversor AD (chip do sensor óptico) vão para o `.bin` gerado. Para o
procedimento de **cadastrar uma marca nova** (arquivo de logo, textos,
etc.), ver `Troca de Logo.txt` na raiz do projeto — este documento aqui é
só sobre como **usar** as marcas e conversores que já existem no firmware.

## 1. Pré-requisitos

- Docker instalado e rodando.
- Terminal comum (PowerShell), não precisa ser administrador.
- Estar na pasta raiz do projeto (`Lummina4Ed Adj`).

## 2. Comando de compilação

```powershell
.\compila_Lummina4EdAdj.ps1 [-Logo NOME] [-Adc NOME]
```

Sem nenhum parâmetro, compila com o padrão: **MAXXIMED + ADS1248**
(hardware/marca de produção atual).

O script roda `idf.py fullclean` + `reconfigure` + `build` dentro do
Docker (`espressif/idf:release-v5.1`), sempre do zero — não precisa
rodar `menuconfig` nem editar nada manualmente. Ao final, o `.bin` fica
em `build\Lummina4_MAXXIMED_v<data>.bin` (o nome do arquivo sempre usa
"MAXXIMED", independente do `-Logo` escolhido — só o conteúdo interno
muda; ver observação na seção 5).

## 3. Parâmetro `-Logo` (marca / ticket impresso)

Controla qual logo aparece na tela web e qual texto sai no ticket
impresso (nome do parceiro e nome do teste — ver `main/printer.cpp` e
`main/branding.cpp`).

| Valor | Logo | Texto no ticket (partner) | Nome do teste impresso |
|---|---|---|---|
| `MAXXIMED` (padrão) | Maxximed | MAXXIMED LATIN AMERICA / Maxximed | Clicktest |
| `BIOSTERONS` | BioSterons | BIOSTERONS / BioSterons | BioSterons |
| `MEDCONTROL` | MedControl | MEDCONTROL / Medcontrol | **BI - Test** |
| `TECHSTERI` | TechSteri | TECHSTERI / Techsteri | Clicktest |
| `BAUMER` | Baumer | BAUMER / Baumer | Clicktest |

Exemplo:

```powershell
.\compila_Lummina4EdAdj.ps1 -Logo MEDCONTROL
```

Os textos de MEDCONTROL, TECHSTERI e BAUMER foram recuperados da versão
do firmware anterior à refatoração para `branding.cpp`
(`D:\Github\ECK\Maxximed\lumina4\main\printer.cpp`), onde cada marca
tinha esses mesmos textos hardcoded por `#ifdef`/comparação de string.

## 4. Parâmetro `-Adc` (conversor AD instalado na placa)

Controla qual driver do sensor óptico é compilado (`CONFIG_ADC_CHIP_*`,
`main/light_sensor.cpp` vs `main/light_sensor_ads1248.cpp`). **As duas
opções não coexistem no mesmo `.bin`** — escolher o chip errado pro
hardware físico da placa faz o sensor óptico não funcionar.

| Valor | Chip | Situação |
|---|---|---|
| `ADS1248` (padrão) | Texas Instruments | Placa atual em produção/bancada, migrada de bit-bang pra `spi_master`, validada fisicamente |
| `CS5534` | Cirrus Logic | Placa antiga, driver recebeu a mesma técnica de leitura do ADS1248 por simetria de código (28/08), mas **sem validação física** — sem hardware CS5534 disponível pra testar |

Exemplo:

```powershell
.\compila_Lummina4EdAdj.ps1 -Adc CS5534
```

**Cuidado:** o `sdkconfig` (arquivo na raiz do projeto, versionado no
git) também guarda essa escolha — se alguém rodar `idf.py menuconfig`
manualmente e trocar o conversor por lá, isso fica persistido no
`sdkconfig` até a próxima vez que o script rodar (que sempre força de
volta pro `-Adc` passado, ou pro padrão ADS1248 se nenhum for passado).
Então, se o build sair com o chip errado, confirme qual conversor foi
usado olhando as 2 linhas em `sdkconfig`:

```
grep ADC_CHIP sdkconfig
```

Deve aparecer exatamente uma das duas com `=y`:
`CONFIG_ADC_CHIP_ADS1248` ou `CONFIG_ADC_CHIP_CS5534`.

## 5. Gravar no equipamento

```powershell
.\flash_Lummina4EdAdj.ps1 -Port COM20
```

(aceita só o número também, ex. `-Port 20`). Pega automaticamente o
`.bin` mais recente em `build\`.

## 6. Observação sobre o nome do `.bin`

O nome do arquivo gerado (`Lummina4_MAXXIMED_v<data>.bin`) sempre usa
"MAXXIMED" no nome, mesmo compilando com `-Logo MEDCONTROL` ou outra
marca — é uma inconsistência pré-existente no `CMakeLists.txt` raiz (a
variável que monta o nome do arquivo não lê o mesmo `-Logo` que o
`branding.cpp` usa em tempo de execução). Não afeta o conteúdo do
firmware, só o nome do arquivo — pra saber com qual `-Logo`/`-Adc` um
`.bin` específico foi gerado, é preciso guardar essa informação por
fora (ex. renomear o arquivo depois do build, ou anotar no histórico da
sessão).
