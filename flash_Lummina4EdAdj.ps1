param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [int]$Baud = 460800
)

# Aceita "7" ou "COM7"
if ($Port -match '^\d+$') {
    $Port = "COM$Port"
}

$Boot = "build\bootloader\bootloader.bin"
$Part = "build\partition_table\partition-table.bin"

$Bin = Get-ChildItem "build\*.bin" |
    Where-Object {
        $_.Name -notmatch "bootloader|partition"
    } |
    Sort-Object Name |
    Select-Object -First 1

if (-not $Bin) {
    Write-Host "ERRO: bin principal não encontrado em build\\" -ForegroundColor Red
    Read-Host
    exit 1
}

$BinPath = $Bin.FullName

Write-Host ""
Write-Host "========================================"
Write-Host " ESP32 FLASH (HOST - ESPTOOL 5.x)"
Write-Host " Porta : $Port"
Write-Host " Baud  : $Baud"
Write-Host " BIN   : $($Bin.Name)"
Write-Host "========================================"
Write-Host ""

# Verificações
foreach ($f in @($BinPath, $Boot, $Part)) {
    if (!(Test-Path $f)) {
        Write-Host "ERRO: Arquivo não encontrado:" -ForegroundColor Red
        Write-Host "  $f"
        Read-Host
        exit 1
    }
}

# Flash (CLI nova do esptool 5.x)
py -m esptool `
    --chip esp32s3 `
    --port $Port `
    --baud $Baud `
    --before default-reset `
    --after hard-reset `
    write-flash `
    --flash-mode dio `
    --flash-freq 80m `
    --flash-size 8MB `
    0x0     $Boot `
    0x8000  $Part `
    0x10000 $BinPath

Write-Host ""
Write-Host "========================================"
Write-Host " Flash finalizado. Pressione ENTER."
Write-Host "========================================"
Read-Host
