# Teste comparativo ADS1248 (spi_master) vs CS5534 — cavidade 1, ampolas de 07/08

**Data:** 11/08/2026, ~16:48
**Branch:** `feature/config-web`
**Placas:** ADS1248 na COM21 (firmware com driver `spi_master`, pós-fix do bus-lock em `ads1248_rdata()`), CS5534 na COM20 (firmware de referência, sem alteração nesta investigação).
**Ampolas:** as mesmas usadas no teste de 07/08 (ver `historico/2026-08-07-comparacao-ads1248-vs-cs5534.md`), cavidade 1 nas duas placas, ciclo de 5 minutos.
**Logs brutos:** `historico/Testes/monitor_com20_cs5534_comparacao.log` (CS5534), `historico/Testes/monitor_com21_ads1248_comparacao.log` (ADS1248).

## Resultado

| | ADS1248 spi_master (COM21) | CS5534 (COM20) |
|---|---|---|
| Leituras no ciclo | 74 | 74 |
| Raw mín | 22911 | 1904 |
| Raw máx | 25973 | 2217 |
| Raw médio | 23264,4 | 1950,1 |
| Positive Percentage | 40,0% | 40,0% |
| Resultado final (Is Positive) | 0 (negativo) | 0 (negativo) |

Classificação final idêntica nas duas placas (negativo, 40% positivo), mesmo padrão do teste de 07/08. Zero reset (`rst:0x`), zero `abort()`/panic/backtrace em qualquer uma das duas capturas — teste completo, `TESTE FINALIZADO COM SUCESSO` nas duas.

**Diferença de escala vs 07/08**: no teste de 07/08 (driver bit-bang), ADS1248 e CS5534 tinham médias muito próximas (4159,1 vs 4143,3, ~0,4% de diferença). Hoje, com o ADS1248 já migrado pra `spi_master`, a proporção ADS1248/CS5534 ficou em **~11,9x**. Mas o CS5534 (que não foi alterado nesta investigação) também mudou de escala em relação a 07/08 — caiu de 4143,3 pra 1950,1 (quase metade) — o que sugere que a diferença de escala não é um bug do driver `spi_master`, e sim de condição física do teste (ampola/reagente com mais dias de uso desde 07/08, iluminação ambiente, intensidade do LED UV), afetando os dois chips.

## Valores brutos (raw) — todas as 74 leituras, na ordem capturada

| # | ADS1248 (COM21) | CS5534 (COM20) |
|---|---|---|
| 1 | 25973 | 2217 |
| 2 | 25395 | 2171 |
| 3 | 25329 | 2140 |
| 4 | 25032 | 2116 |
| 5 | 24777 | 2100 |
| 6 | 24579 | 2083 |
| 7 | 24400 | 2070 |
| 8 | 24218 | 2057 |
| 9 | 24073 | 2045 |
| 10 | 23888 | 2032 |
| 11 | 23783 | 2023 |
| 12 | 23642 | 2012 |
| 13 | 23558 | 2004 |
| 14 | 23441 | 1995 |
| 15 | 23336 | 1984 |
| 16 | 23236 | 1978 |
| 17 | 23148 | 1970 |
| 18 | 23060 | 1961 |
| 19 | 23011 | 1953 |
| 20 | 22957 | 1947 |
| 21 | 22937 | 1939 |
| 22 | 22921 | 1930 |
| 23 | 22920 | 1928 |
| 24 | 22911 | 1925 |
| 25 | 22914 | 1924 |
| 26 | 22914 | 1921 |
| 27 | 22920 | 1923 |
| 28 | 22915 | 1922 |
| 29 | 22914 | 1918 |
| 30 | 22928 | 1920 |
| 31 | 22917 | 1921 |
| 32 | 22917 | 1917 |
| 33 | 22927 | 1919 |
| 34 | 22932 | 1919 |
| 35 | 22942 | 1918 |
| 36 | 22950 | 1921 |
| 37 | 22955 | 1916 |
| 38 | 22949 | 1922 |
| 39 | 22975 | 1920 |
| 40 | 22964 | 1918 |
| 41 | 22975 | 1921 |
| 42 | 22949 | 1915 |
| 43 | 22989 | 1919 |
| 44 | 23006 | 1913 |
| 45 | 23001 | 1916 |
| 46 | 23002 | 1919 |
| 47 | 23008 | 1919 |
| 48 | 23002 | 1912 |
| 49 | 23021 | 1916 |
| 50 | 23005 | 1917 |
| 51 | 23010 | 1913 |
| 52 | 23012 | 1911 |
| 53 | 23022 | 1916 |
| 54 | 23025 | 1915 |
| 55 | 23021 | 1912 |
| 56 | 23035 | 1912 |
| 57 | 23033 | 1912 |
| 58 | 23026 | 1912 |
| 59 | 23036 | 1912 |
| 60 | 23012 | 1913 |
| 61 | 23000 | 1910 |
| 62 | 23010 | 1909 |
| 63 | 23019 | 1908 |
| 64 | 23011 | 1912 |
| 65 | 22989 | 1910 |
| 66 | 22983 | 1908 |
| 67 | 22997 | 1908 |
| 68 | 22976 | 1911 |
| 69 | 22992 | 1904 |
| 70 | 22985 | 1908 |
| 71 | 22995 | 1908 |
| 72 | 22985 | 1908 |
| 73 | 22985 | 1904 |
| 74 | 22989 | 1905 |

## Padrão observado

Ambos os chips seguem o mesmo formato de curva: queda acentuada nas primeiras ~20-25 leituras (efeito de estabilização inicial/exposição UV), depois estabilizam num patamar com variação pequena pelo resto do ciclo. O comportamento relativo (forma da curva) é equivalente entre os dois chips, só a escala absoluta difere.

## Pendente para a próxima sessão

Esse teste comparou ADS1248 (spi_master) vs CS5534 — mas o CS5534 não é um bom controle isolado pra explicar a diferença de escala, porque é um chip diferente com característica analógica própria (não só driver diferente). Pra isolar se a diferença de raw vem do driver (`spi_master` vs bit-bang) ou de condição física do teste (ampola, iluminação, etc.), o teste que falta é:

**Comparação entre 2 placas ADS1248** (mesmo chip, mesma característica analógica) — uma rodando `spi_master`, a outra com o driver bit-bang original (versão do commit `191f7e1`, antes da migração) — mesma ampola, mesma cavidade, ao mesmo tempo, do mesmo jeito que esse teste fez ADS1248 vs CS5534. Se as duas ADS1248 baterem próximas uma da outra (como bit-bang batia com CS5534 em 07/08), a diferença de escala é do driver. Se ainda ficarem bem diferentes entre si, reforça que é condição física do teste, não o driver.

Precisa de 2 placas ADS1248 físicas disponíveis ao mesmo tempo pra esse teste (hoje só uma estava disponível, comparada contra a CS5534).
