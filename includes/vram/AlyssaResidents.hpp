// =============================================================================
// AlyssaResidents.hpp — implementações concretas de IVRAMResident para os
// backends do projeto (Fases 1 e 3 do plano).
//
//   WhisperResident — TIER2_JIT, id "whisper". Ponte para o load/unload do
//                     modelo dentro do VoicePipeline (whisper.cpp).
//   LLMResident     — TIER1_HOT, id "llm". O modelo é carregado pelo
//                     CoreIntegration::initialize() e FICA residente; load é
//                     idempotente (só confirma/mede) e unload é recusado até
//                     a Fase 5 (eviction) existir.
//   TTSResident     — TIER2_JIT, id "tts". Fase 3: backend local = KokoroTTS
//                     (ONNX Runtime CPU-only → footprint de VRAM 0; o load
//                     gerencia a sessão em RAM). As lambdas vêm do CLI de voz;
//                     o scheduler não mudou (esse é o ponto do contrato).
// =============================================================================

#pragma once

#include "ResidentBase.hpp"

#include <functional>

class VoicePipeline; // includes/voice/VoicePipeline.hpp

namespace alyssa::vram {

class WhisperResident : public ResidentBase {
public:
    /// @param pipeline VoicePipeline vivo durante toda a vida do resident.
    /// @param footprint_bytes Estimativa de VRAM (ver plano_scheduler/BASELINE.md).
    /// @param tier A Fase 0 mediu large-v3 em ~3.5GiB / ~2.7s de load por ciclo
    ///        JIT. Como o orçamento comporta tudo residente (~12GiB/16GiB),
    ///        TIER1_HOT elimina esses 2.7s por turno — o default segue o plano
    ///        (JIT), mas a troca é um argumento aqui, sem tocar no scheduler.
    WhisperResident(VoicePipeline* pipeline, std::size_t footprint_bytes,
                    ResidencyTier tier = ResidencyTier::TIER2_JIT);

protected:
    bool do_load() override;
    bool do_unload() override;

private:
    VoicePipeline* pipeline_;
};

class LLMResident : public ResidentBase {
public:
    /// @param footprint_bytes_getter Ex.: [&]{ return brain.llm_vram_footprint_bytes(); }
    ///        Retornar 0 = modelo ainda não inicializado (load falha).
    explicit LLMResident(std::function<std::size_t()> footprint_bytes_getter);

protected:
    bool do_load() override;
    bool do_unload() override;

private:
    std::function<std::size_t()> footprint_getter_;
};

class TTSResident : public ResidentBase {
public:
    using Op = std::function<bool()>;

    /// Sem argumentos = stub (TTS via API, zero VRAM). Com lambdas = backend
    /// local de verdade — Fase 3: KokoroTTS (ONNX/CPU, footprint de VRAM 0).
    /// @param tier Medição da Fase 3: load quente ~1.5s e VRAM ZERO → residência
    ///        não custa nada do orçamento; o CLI de voz promove a TIER1_HOT
    ///        (mesma lógica da promoção do Whisper — ver BASELINE.md).
    TTSResident(std::size_t footprint_bytes = 0, Op loader = {}, Op unloader = {},
                ResidencyTier tier = ResidencyTier::TIER2_JIT);

protected:
    bool do_load() override;
    bool do_unload() override;

private:
    Op loader_;
    Op unloader_;
};

} // namespace alyssa::vram
