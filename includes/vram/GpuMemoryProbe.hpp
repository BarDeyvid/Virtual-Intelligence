// =============================================================================
// GpuMemoryProbe.hpp — leitura da VRAM *real* da GPU (Fase 5).
//
// O incidente de 2026-07-04 (BSOD por 15.6/16GB) provou que contabilizar só
// os próprios residents contra um orçamento fixo é cego: o resto do desktop
// (jogo, browser, LM Studio) compete pela mesma VRAM. Este probe dá ao
// scheduler a verdade do hardware.
//
// Implementação: NVML (nvml.dll / libnvidia-ml.so.1) carregada dinamicamente
// — consulta em ~µs, sem dependência de build (mesma técnica do espeak-ng no
// KokoroTTS). Fallback: `nvidia-smi` via popen (~100ms, só se NVML faltar).
// Sem GPU NVIDIA: retorna nullopt e o scheduler degrada para contabilidade
// própria (comportamento pré-Fase 5).
// =============================================================================

#pragma once

#include <cstddef>
#include <optional>

namespace alyssa::vram {

struct GpuMemInfo {
    std::size_t total_bytes = 0;
    std::size_t free_bytes  = 0;
    std::size_t used_bytes  = 0; // uso GLOBAL da GPU (inclui outros processos)
};

/// Snapshot da memória da GPU 0. Thread-safe; NVML é inicializada uma única
/// vez por processo (lazy). nullopt = sem como medir (sem driver NVIDIA).
[[nodiscard]] std::optional<GpuMemInfo> query_gpu_memory();

} // namespace alyssa::vram
