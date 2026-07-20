# Benchmarks v2 — F1 (comitê aposentado)

**Branch:** v2 · **Data:** 2026-07-19
**Referência v1:** docs/benchmarks-2026-07-11.md (pré-migração Gemma 4; os números
ponta-a-ponta de lá são a baseline do comitê: fast path 330-370ms, comitê 1.5-2.3s).

## O que mudou na F1

- 11 personas 1B removidas do ConfigsLLM.json → sobrou `utility` (1B),
  `alyssa` (E2B) e `gameplayModel` (E2B lazy).
- `think_with_fusion_core`: caminho único memória → `generate_direct_response`
  com histórico. Sem gating, sem pool paralelo, sem fusão, sem coerência.
- Router prepass FORA do caminho quente (volta na F4 como seletor
  reflexo/córtex). `WeightedFusion.cpp` fora do build.
- Fallback 1B (resposta vazia/erro) agora cobre TODO turno, não só o comitê.
- Small talk continua com atalho (pula busca de memória), mas SEMPRE com
  histórico — o fast path sem contexto morreu.
- Hormônios agora reagem à troca real (input+resposta). Na v1 analisavam os
  sinais do comitê em PT-BR contra keywords EN — quase nunca disparava.

## Resultados (preencher com models/ no lugar)

> PENDENTE: os .gguf não estão neste checkout (models/ é gitignored).
> Repor `models/gemma-4-E2B_q4_0-it.gguf` e `models/gemma-3-1b-it-q4_0.gguf`
> e rodar os turnos abaixo via alyssad.

| métrica | v1 (07-11) | v2/F1 | alvo F1 |
|---|---:|---:|---:|
| small talk ("oi") | 330-370 ms | — | ≤ 500 ms |
| turno substantivo | 1.5-2.3 s | — | ≤ 600 ms |
| VRAM stack carregado | ~8.1 GiB | — | ≥ 1.5 GiB a menos |
| persona (10 perguntas cegas) | — | — | sem regressão |

Roteiro dos 6 turnos (mesmos do 07-11): "oi" · "qual sua opinião sobre café de
manhã?" · "tô meio cansado hoje..." · pergunta com tool call (hora/arquivos) ·
pergunta com memória ("lembra do...") · despedida.

## Smoke test estrutural (sem modelos) — RODADO 2026-07-19 ✔

- Build Release limpo (alyssa_cli, alyssad, test_fusion_utils): zero erros,
  só os warnings C4267/C4244 pré-existentes (dívida da Fase 4 std::string).
  Gotcha novo: `models/` sumido quebra o POST-BUILD copy do CMake com
  MSB3073 — o diretório precisa existir mesmo vazio (recriado com README).
- `alyssad --echo` + driver NDJSON (node): ping 2ms, status ok
  (`echo:true`), say → eventos `state:thinking` → `token` → `response` →
  `state:idle`. `say` durante turno devolveu `busy` como documentado.
- `test_fusion_utils`: 33/33 passando (helpers header-only fora do caminho
  quente; ExpertContribution segue como tipo da interface IExpert).
