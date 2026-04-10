# =========================================================
# COMPILACAO ESP32 (DOCKER + ESP-IDF 5.1)
# Projeto: Lummina4Ed 37
# =========================================================
# .\compila_Lummina4Ed37.ps1 -Logo BIOSTERONS
# .\compila_Lummina4Ed37.ps1  (MAXXIMED)

$ErrorActionPreference = "Stop"

# -------------------------------
# Parametro de branding
# -------------------------------
$LOGO = "MAXXIMED"

for ($i = 0; $i -lt $args.Count; $i++) {
    if ($args[$i] -eq "-Logo" -and $i + 1 -lt $args.Count) {
        $LOGO = $args[$i + 1]
        break
    }
}

Write-Host "LOGO selecionado: $LOGO"
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
docker run --rm `
    -v "${PROJECT_DIR}:/work" `
    -v "${CCACHE_DIR}:/ccache" `
    -w /work `
    -e CCACHE_DIR=/ccache `
    $DOCKER_IMAGE `
    idf.py "-DLOGO=$LOGO" reconfigure

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
