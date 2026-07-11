#include "AlyssaResidents.hpp"

#include "../voice/VoicePipeline.hpp"

#include <iostream>

namespace alyssa::vram {

// --- WhisperResident ---------------------------------------------------------

WhisperResident::WhisperResident(VoicePipeline* pipeline, std::size_t footprint_bytes,
                                 ResidencyTier tier)
    : ResidentBase("whisper", tier, footprint_bytes),
      pipeline_(pipeline) {}

bool WhisperResident::do_load() {
    return pipeline_ && pipeline_->load_model();
}

bool WhisperResident::do_unload() {
    if (!pipeline_) return false;
    pipeline_->unload_model();
    return true;
}

// --- LLMResident -------------------------------------------------------------

LLMResident::LLMResident(std::function<std::size_t()> footprint_bytes_getter)
    : ResidentBase("llm", ResidencyTier::TIER1_HOT, 0),
      footprint_getter_(std::move(footprint_bytes_getter)) {}

bool LLMResident::do_load() {
    // O load real acontece no CoreIntegration::initialize(); aqui só
    // confirmamos que ele existe e registramos o footprint medido.
    const std::size_t bytes = footprint_getter_ ? footprint_getter_() : 0;
    if (bytes == 0) {
        std::cerr << "[vram] LLMResident: CoreIntegration ainda não inicializado" << std::endl;
        return false;
    }
    set_footprint(bytes);
    return true;
}

bool LLMResident::do_unload() {
    // TODO(Fase 5): eviction de TIER1_HOT com garantia de reload. Até lá,
    // descarregar o LLM é recusado — a base volta o status para RESIDENT.
    std::cerr << "[vram] LLMResident: unload recusado (eviction de TIER1 é Fase 5)" << std::endl;
    return false;
}

// --- TTSResident -------------------------------------------------------------

TTSResident::TTSResident(std::size_t footprint_bytes, Op loader, Op unloader,
                         ResidencyTier tier)
    : ResidentBase("tts", tier, footprint_bytes),
      loader_(std::move(loader)), unloader_(std::move(unloader)) {}

bool TTSResident::do_load() {
    if (loader_) return loader_();
    // Sem lambdas = stub (ex.: TTS via API, zero VRAM). O caminho real da
    // Fase 3 (Kokoro/ONNX) entra pelas lambdas no Alyssa_CLI_WITH_VOICE.cpp.
    std::cout << "[vram] TTSResident: stub (sem backend, nada a carregar)" << std::endl;
    return true;
}

bool TTSResident::do_unload() {
    if (unloader_) return unloader_();
    return true;
}

} // namespace alyssa::vram
