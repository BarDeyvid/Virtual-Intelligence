# espeak-ng (G2P do Kokoro TTS) — artefatos locais

O `KokoroTTS` (includes/voice/KokoroTTS.cpp) usa a **libespeak-ng.dll** por
carga dinâmica (LoadLibrary + 3 funções) para fonemizar pt-br em IPA.
A DLL e os modelos são **gitignored** (`*.dll`, `models/`) — este README
documenta como reobter tudo numa máquina nova.

## libespeak-ng.dll (fica nesta pasta)

Extraída do MSI oficial do espeak-ng **1.52.0**, sem instalar:

```powershell
curl -LO https://github.com/espeak-ng/espeak-ng/releases/download/1.52.0/espeak-ng.msi
msiexec /a espeak-ng.msi /qn TARGETDIR=C:\tmp\espeak-extract
# copiar de "C:\tmp\espeak-extract\eSpeak NG\":
#   libespeak-ng.dll   → third_party/espeak-ng/
#   espeak-ng-data\    → models/kokoro/espeak-ng-data/
```

O CMake copia a DLL para o diretório do executável no post-build do
`alyssa_cli`; o espeak-ng-data vai junto com a cópia de `models/`.

Licença do espeak-ng: GPL-3.0 (https://github.com/espeak-ng/espeak-ng).

## models/kokoro/ (modelo + vozes + vocab)

De https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX (Apache-2.0):

```
onnx/model.onnx            → models/kokoro/model.onnx            (fp32, DEFAULT — 3x mais rápido que o int8 na CPU, ver BASELINE.md)
onnx/model_quantized.onnx  → models/kokoro/model_quantized.onnx  (int8, opcional)
voices/pf_dora.bin         → models/kokoro/voices/pf_dora.bin    (voz pt-br feminina)
voices/pm_alex.bin         → models/kokoro/voices/pm_alex.bin    (voz pt-br masculina)
```

E o vocab (mapa fonema IPA → token id) de
https://huggingface.co/hexgrad/Kokoro-82M/resolve/main/config.json
→ `models/kokoro/config.json`.

Medições de load/RAM/latência: `test_kokoro` (tests/test_kokoro.cpp),
resultados em plano_scheduler/BASELINE.md (seção Fase 3).
