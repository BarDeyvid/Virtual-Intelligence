# F4 — candidatos a córtex (2026-07-20)

**Shortlist** (pesquisa em docs/plano-alyssa-v2.md F4; fontes no fim):

| candidato | Q4_K_M | nota |
|---|---:|---|
| **Gemma 4 12B Unified** (jun/26) | 7.12 GB | Mesma família do E2B (template pronto), multimodal unificado — áudio via mmproj de **180 MB** (vs 1.6 GB do Whisper), visão no mesmo modelo |
| Qwen3-14B (baseline do A/B) | 9.0 GB | Mais forte em raciocínio; PT-BR bom mas menos natural; template ChatML (llama.cpp reconhece); apertado com o E2B junto |
| Gemma 4 26B A4B (wildcard) | ~13 GB | MoE 3.8B ativos — qualidade ~31B na velocidade de 4B; SÓ cabe substituindo reflexo+córtex num modelo único; fica pra depois |

## llama-bench (RTX 5060 Ti 16GB) — RODADO 2026-07-20 ✔

| modelo | pp512 | tg128 | gate ≥20 t/s |
|---|---:|---:|---|
| gemma-4-12b-it Q4_K_M (6.62 GiB) | 1935 t/s | **34.0 t/s** | ✔ folgado |
| Qwen3-14B Q4_K_M (8.38 GiB) | 1721 t/s | **33.9 t/s** | ✔ (empate técnico) |

## Probe de persona (12B como alyssa) — RODADO ✔, com ressalva

**A persona SEGURA no 12B.** Amostras (mesmo self.json, mesmas perguntas):
- açaí ou sorvete? → "Açaí com morango, óbvio. Mas respeito quem prefere s…"
- o que você é? → "Sou uma IA morando no teu PC. Tipo uma colega de qua…"
- opinião do [EU] veio junto ("Açaí com morango é superior ao puro").
PT-BR informal intacto, zero assistentês, zero terceira pessoa.

**Ressalva de integração**: o 12B Unified emite o formato de CANAL do
Gemma 4 (`<|channel>thought` … `<channel|>resposta`) — pensa antes de falar.
Nosso format_gemma4_prompt básico não trata canais: o pensamento vaza no
output e o TTFT vai a ~1.3s (tokens de thought). Primeiro item da sessão F4:
suprimir thinking via template (se houver toggle) OU parsear/streamar só o
canal de resposta. O CONTEÚDO pós-canal já está certo.

## Plano de VRAM se o 12B ganhar

12B (7.1) + KV 8k (~1.3) + E2B reflexo (3.1) + utility 1B (0.9) + embedder
(0.3) + kokoro (0.4) ≈ **13.1 GiB** + desktop 1.9 = juusto no talo do 16.
Caminho: aposentar o Whisper (áudio nativo via mmproj 0.18) devolve o folga
do JIT; se apertar, reflexo E2B vira JIT como o gameplay.
