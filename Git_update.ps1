<#
.SYNOPSIS
Atualiza o repositório Git do Lummina 4 (add, commit e push).

.DESCRIPTION
Script para padronizar o fluxo de atualização do repositório Lummina4Ed.
Executa validações, git add -A, commit (mensagem automática ou manual)
e push para o upstream. Opcionalmente faz pull --rebase.

.EXAMPLE
.\git_update.ps1 -RepoPath "D:\Github\ECK\Maxximed\Lummina4Ed"

Commit + push com mensagem automática.

.EXAMPLE
.\git_update.ps1 -RepoPath "D:\Github\ECK\Maxximed\Lummina4Ed" `
  -Message "fix: branding logo select via define"

Commit + push com mensagem personalizada.

.EXAMPLE
.\git_update.ps1 -RepoPath "D:\Github\ECK\Maxximed\Lummina4Ed" -PullRebase

Pull --rebase antes do commit/push.

.EXAMPLE
.\git_update.ps1 -RepoPath "D:\Github\ECK\Maxximed\Lummina4Ed" -NoPush

Somente commit (sem push).
#>


param(
  [string]$RepoPath = ".",
  [string]$Message = "",
  [switch]$PullRebase,
  [switch]$NoPush
)

$ErrorActionPreference = "Stop"

function Die($msg) {
  Write-Host "ERRO: $msg" -ForegroundColor Red
  exit 1
}

# 1) Ir para o repo
if (!(Test-Path $RepoPath)) { Die "Caminho não existe: $RepoPath" }
Push-Location $RepoPath

try {
  # 2) Verificar git
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Die "git não encontrado no PATH." }

  # 3) Verificar se é repo
  $isRepo = git rev-parse --is-inside-work-tree 2>$null
  if ($LASTEXITCODE -ne 0 -or $isRepo.Trim() -ne "true") { Die "Este diretório não é um repositório git." }

  # 4) Info básica
  $branch = (git rev-parse --abbrev-ref HEAD).Trim()
  Write-Host "Repo: $(Resolve-Path .)"
  Write-Host "Branch: $branch"
  Write-Host ""

  # 5) Status (antes)
  Write-Host "Status (antes):"
  git status --porcelain
  Write-Host ""

  # 6) Pull rebase (opcional)
  if ($PullRebase) {
    Write-Host "Pull com rebase..."
    git pull --rebase
    if ($LASTEXITCODE -ne 0) { Die "Falha no pull --rebase (conflito ou erro remoto)." }
    Write-Host ""
  }

  # 7) Add
  git add -A
  if ($LASTEXITCODE -ne 0) { Die "Falha no git add -A." }

  # 8) Commit (se houver mudanças staged)
  $staged = git diff --cached --name-only
  if ([string]::IsNullOrWhiteSpace($staged)) {
    Write-Host "Nada para commitar (working tree limpa ou sem staged)."
  } else {
    if ([string]::IsNullOrWhiteSpace($Message)) {
      $dt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
      $Message = "update: Lummina4 ($dt)"
    }

    Write-Host "Commit: $Message"
    git commit -m "$Message"
    if ($LASTEXITCODE -ne 0) { Die "Falha no commit (hooks? conflito? config?)." }
  }

  # 9) Push
  if (-not $NoPush) {
    # garante upstream
    $upstream = git rev-parse --abbrev-ref --symbolic-full-name "@{u}" 2>$null
    if ($LASTEXITCODE -ne 0) {
      Write-Host "Sem upstream configurado. Tentando configurar para origin/$branch ..."
      git push -u origin $branch
      if ($LASTEXITCODE -ne 0) { Die "Falha ao configurar upstream / push inicial." }
    } else {
      Write-Host "Push para $upstream ..."
      git push
      if ($LASTEXITCODE -ne 0) { Die "Falha no push." }
    }
  } else {
    Write-Host "NoPush habilitado: não foi feito push."
  }

  # 10) Status final
  Write-Host ""
  Write-Host "Status (final):"
  git status
}
finally {
  Pop-Location
}
