# Script auxiliar temporario - compila as 8 combinacoes (4 logos x 2 ADCs)
# e copia cada .bin renomeado para build_releases\
$ErrorActionPreference = "Stop"

$logos = @("MAXXIMED", "BIOSTERONS", "MEDCONTROL", "TECHSTERI", "BAUMER")
$logoDisplay = @{
    "MAXXIMED"  = "Maxximed"
    "BIOSTERONS"= "BioSterons"
    "MEDCONTROL"= "MedControl"
    "TECHSTERI" = "TechSteri"
    "BAUMER"    = "Baumer"
}
$adcs = @("ADS1248", "CS5534")

$destDir = Join-Path (Get-Location).Path "build_releases"
if (!(Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir | Out-Null }

foreach ($adc in $adcs) {
    foreach ($logo in $logos) {
        Write-Host "===================================================="
        Write-Host "BUILD: Logo=$logo Adc=$adc"
        Write-Host "===================================================="

        & .\compila_Lummina4EdAdj.ps1 -Logo $logo -Adc $adc
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERRO no build Logo=$logo Adc=$adc (exit $LASTEXITCODE)"
            exit 1
        }

        $bin = Get-ChildItem -Path "build" -Filter "*.bin" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (-not $bin) {
            Write-Host "ERRO: nenhum .bin encontrado em build\ apos compilar Logo=$logo Adc=$adc"
            exit 1
        }

        $dateStamp = Get-Date -Format "yyyy_MM_dd"
        $destName = "Lummina4_v${dateStamp}_${adc}_$($logoDisplay[$logo]).bin"
        $destPath = Join-Path $destDir $destName

        Copy-Item -Path $bin.FullName -Destination $destPath -Force
        Write-Host "Copiado: $destPath"
    }
}

Write-Host ""
Write-Host "Todos os builds concluidos."
Get-ChildItem $destDir | Format-Table Name, Length, LastWriteTime
