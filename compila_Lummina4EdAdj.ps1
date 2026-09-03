# =========================================================
# COMPILACAO ESP32 (DOCKER + ESP-IDF 5.1)
# Projeto: Lummina4Ed Adj
# =========================================================
# .\compila_Lummina4EdAdj.ps1 -Logo BIOSTERONS -Adc CS5534
# .\compila_Lummina4EdAdj.ps1  (MAXXIMED + ADS1248, padrao)

$ErrorActionPreference = "Stop"

# -------------------------------
# Parametro de branding
# -------------------------------
$LOGO = "MAXXIMED"

# -------------------------------
# Parametro de conversor AD (ADS1248 = placa atual em producao/bancada;
# CS5534 = placa antiga - ver main/Kconfig.projbuild)
# -------------------------------
$ADC = "ADS1248"

for ($i = 0; $i -lt $args.Count; $i++) {
    if ($args[$i] -eq "-Logo" -and $i + 1 -lt $args.Count) {
        $LOGO = $args[$i + 1]
    }
    if ($args[$i] -eq "-Adc" -and $i + 1 -lt $args.Count) {
        $ADC = $args[$i + 1]
    }
}

$ADC = $ADC.ToUpper()
if ($ADC -ne "ADS1248" -and $ADC -ne "CS5534") {
    Write-Host "ERRO: -Adc precisa ser ADS1248 ou CS5534 (recebido: $ADC)"
    exit 1
}

Write-Host "LOGO selecionado: $LOGO"
Write-Host "Conversor AD selecionado: $ADC"
Write-Host ""

# -------------------------------
# Paths
# -------------------------------
$PROJECT_DIR = (Get-Location).Path
$CCACHE_DIR  = "$env:LOCALAPPDATA\esp-idf-ccache"

if (!(Test-Path $CCACHE_DIR)) {
    New-Item -ItemType Directory -Path $CCACHE_DIR | Out-Null
}

# -------------------------------
# Docker image
# -------------------------------
$DOCKER_IMAGE = "espressif/idf:release-v5.1"

# -------------------------------
# Forca o conversor AD selecionado direto nas 2 linhas do choice
# ADC_CHIP no sdkconfig - sem isso, o fullclean/reconfigure abaixo pode
# manter (ou silenciosamente perder) uma selecao feita antes via
# menuconfig, e nao existe outro jeito confiavel de passar um choice do
# Kconfig por linha de comando pro idf.py.
# -------------------------------
$SDKCONFIG_PATH = Join-Path $PROJECT_DIR "sdkconfig"

if (Test-Path $SDKCONFIG_PATH) {
    $content = Get-Content $SDKCONFIG_PATH -Raw

    function Set-KconfigBool([string]$text, [string]$name, [bool]$enabled) {
        # sdkconfig usa CRLF - o \r sobra entre o fim do texto da linha e o
        # \n, entao o $ (fim de linha, modo multiline) precisa contar com
        # ele explicitamente ou o pattern nunca casa (bug real: rodou
        # silenciosamente sem trocar nada em 2 testes antes de eu notar).
        $pattern = "(?m)^(?:# )?$name(?:=y)?(?: is not set)?[ \t]*\r?$"
        $replacement = if ($enabled) { "$name=y" } else { "# $name is not set" }
        return [System.Text.RegularExpressions.Regex]::Replace($text, $pattern, $replacement)
    }

    $content = Set-KconfigBool $content "CONFIG_ADC_CHIP_CS5534" ($ADC -eq "CS5534")
    $content = Set-KconfigBool $content "CONFIG_ADC_CHIP_ADS1248" ($ADC -eq "ADS1248")

    Set-Content -Path $SDKCONFIG_PATH -Value $content -NoNewline
}

# -------------------------------
# FULLCLEAN
# -------------------------------
docker run --rm `
    -v "${PROJECT_DIR}:/work" `
    -v "${CCACHE_DIR}:/ccache" `
    -w /work `
    -e CCACHE_DIR=/ccache `
    $DOCKER_IMAGE `
    idf.py fullclean

# -------------------------------
# RECONFIGURE
# -------------------------------
# SDKCONFIG_DEFAULTS carrega o overlay do ADC por cima do sdkconfig.defaults
# base - necessario porque o fullclean acima apaga o sdkconfig existente
# (o patch direto feito antes do fullclean nao sobrevive a isso), entao o
# reconfigure regenera do zero e precisa do overlay pra nao cair de volta
# no default do sdkconfig.defaults.
$ADC_OVERLAY = "sdkconfig.adc." + $ADC.ToLower()

docker run --rm `
    -v "${PROJECT_DIR}:/work" `
    -v "${CCACHE_DIR}:/ccache" `
    -w /work `
    -e CCACHE_DIR=/ccache `
    $DOCKER_IMAGE `
    idf.py "-DLOGO=$LOGO" "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;$ADC_OVERLAY" reconfigure

# -------------------------------
# BUILD
# -------------------------------
docker run --rm `
    -v "${PROJECT_DIR}:/work" `
    -v "${CCACHE_DIR}:/ccache" `
    -w /work `
    -e CCACHE_DIR=/ccache `
    $DOCKER_IMAGE `
    idf.py build
