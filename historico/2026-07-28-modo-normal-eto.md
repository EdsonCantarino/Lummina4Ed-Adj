# Mudanças desta versão — Modo de Operação Normal/ETO selecionável via web

**Data:** 28/07/2026
**Branch:** `feature/config-web`
**Commit:** `e155da2`
**Motivo:** cliente pediu dois modos de teste selecionáveis via web (checkboxes mutuamente exclusivos, estilo radio, Normal marcado por padrão):
- **Normal** — tempo selecionável nos botões da frente (20min/1h/2h/3h), com beep e LED de nível a cada toque.
- **ETO** — tempo fixo de 20min, botões de tempo da frente sem nenhuma reação (sem beep, sem LED, sem chamada de `ampoule_set_time_test`).

Plano completo salvo em `C:\Users\Edson\.claude\plans\dreamy-plotting-crab.md`.

## Alterado

- `components/advanced_config/advanced_config.h` / `.cpp` — novo campo `eto_mode` na struct `advanced_config_t` (persistido como blob bruto com CRC na NVS, duas áreas `read_area`/`write_area`).
- `main/app_httpd.cpp` — `GET`/`POST /api/v1/advanced_config` passam a ler/gravar `etoMode`.
- `main/keyboard.cpp` — `button_event_task` e `update_button_level` condicionados a `g_advanced_config.eto_mode`: em ETO, ignoram completamente o toque nos botões de tempo.
- `main/ampoule_test.cpp` — restaurado o mapeamento nível→tempo em `ampoule_set_time_test` (usado só no modo Normal).
- `components/httpd_app/www/pages/advanced_config.html` + os 3 locales (`.json`) e seus `.gz` regenerados — novo card "Modo de Operação" com os dois checkboxes mutuamente exclusivos.
- `CMakeLists.txt` — timestamp automático de versão (pedido do usuário nesta sessão).

## Consequência conhecida — reset de NVS em unidades já em campo

Adicionar o campo `eto_mode` muda o tamanho do blob salvo (`advanced_config_blob_t`). A checagem de integridade da NVS inclui `blob.size() != sizeof(advanced_config_blob_t)` — então qualquer unidade que já tinha configuração salva por firmware anterior vai ver essa config como "tamanho inesperado" no próximo boot, cair pro padrão de fábrica (perdendo temperatura/looping/amostras customizados) e mostrar o alerta de inconsistência até o operador clicar em "Restaurar Padrão de Fábrica". **Não é bug, é esperado** — mas avisar o cliente antes de atualizar firmware em unidades já configuradas em campo.

## Testado e confirmado funcionando (28/07, equipamento real — COM20, AP 192.168.10.10)

- Build via Docker (`espressif/idf:release-v5.1`) ok, flash ok.
- Card "Modo de Operação" renderiza certo, Normal marcado por padrão.
- Checkboxes mutuamente exclusivos funcionando.
- Salvar e Restaurar padrão de fábrica funcionando (POST confirmado, NVS grava as 2 áreas, GET reflete `etoMode` corretamente).
- **Normal:** beep + LED do painel + tempo real (1h/2h/3h/20min) batendo com o nível escolhido, confirmado via serial (`LEVEL n` / `TIME xxxx`) e confirmação visual/sonora do usuário.
- **ETO:** nenhuma reação nos botões de tempo (sem beep, sem LED, sem chamada de `ampoule_set_time_test` — ausência total de "LEVEL/TIME" na serial durante vários toques).
- Cancelamento de slot/ampola funcionando normalmente nos dois modos.
- Salvar/Restaurar padrão de fábrica persistindo corretamente nas 2 áreas NVS.

## Pendente

- Nenhuma pendência de código — recurso completo, validado fisicamente e commitado.
- `.claude/settings.local.json` ficou de fora do commit (config local de permissões da sessão, não faz parte do recurso).
