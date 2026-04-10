# Comandos Git Básicos (ECK Engenharia)

## Atualizar o projeto antes de trabalhar
Sempre fazer antes de começar qualquer edição:
git pull

## Verificar alterações feitas
git status

## Adicionar todos os arquivos modificados
git add .

## Criar commit descrevendo a mudança
git commit -m "descreva aqui o que foi feito"

## Enviar para o GitHub
git push

## Histórico resumido (para ver mudanças anteriores)
git log --oneline --graph

## Resumo em uma linha
git pull && git add . && git commit -m "update" && git push

## Criar nova branch (caso necessário para testes)
git checkout -b nome-da-branch

## Voltar para a branch principal
git checkout main
