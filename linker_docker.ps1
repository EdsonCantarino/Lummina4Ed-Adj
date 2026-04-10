Write-Host ""
Write-Host "========================================"
Write-Host " SHELL ESP-IDF (DOCKER)"
Write-Host " Projeto montado em /work"
Write-Host "========================================"
Write-Host ""

if (-not (Test-Path ".\CMakeLists.txt")) {
    Write-Host "ERRO: CMakeLists.txt nao encontrado."
    Write-Host "Execute este script na raiz do projeto ESP-IDF."
    Read-Host "Pressione ENTER para sair"
    exit 1
}

Write-Host " Comandos uteis:"
Write-Host "   idf.py menuconfig"
Write-Host "   idf.py build"
Write-Host "   idf.py size"
Write-Host "========================================"
Write-Host ""

docker run --rm -it -v "$($PWD.Path):/work" -w /work espressif/idf:release-v5.1 bash

Write-Host ""
Write-Host "========================================"
Write-Host " Shell encerrado. Pressione ENTER."
Write-Host "========================================"
Read-Host
