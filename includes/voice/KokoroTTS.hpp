// KokoroTTS.hpp — TTS local (Kokoro-82M, ONNX Runtime) substituindo o
// ElevenLabs no pipeline de voz (Fase 3 do plano_scheduler/PLANO.md).
//
// Cadeia: texto → espeak-ng (G2P pt-br, IPA) → pós-processo estilo misaki →
// tokens do vocab do Kokoro → ONNX (input_ids, style, speed) → waveform
// 24kHz mono float32 → PortAudio.
//
// O ONNX Runtime do vcpkg é CPU-only (sem provider CUDA): a inferência roda
// na CPU e o footprint de VRAM é ZERO — ver plano_scheduler/BASELINE.md.
// load()/unload() existem para o TTSResident (TIER2_JIT) do scheduler:
// gerenciam a sessão ONNX em RAM, não VRAM.
//
// espeak-ng: libespeak-ng.dll carregada dinamicamente (LoadLibrary) — sem
// import lib, sem header; só 4 funções via GetProcAddress. Os dados ficam
// em models/kokoro/espeak-ng-data. Uma vez inicializado, o espeak fica no
// processo (poucos MB); ciclos init/terminate repetidos não valem o risco.
#ifndef KOKORO_TTS_HPP
#define KOKORO_TTS_HPP

#include "TTSBase.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ort {
struct Env;
struct Session;
} // namespace Ort

class KokoroTTS : public ITTS {
public:
    struct Config {
        // fp32 mesmo: medido 3x MAIS RÁPIDO que o model_quantized.onnx int8
        // na CPU (RTF 0.28 vs 0.87 no 5800X) — os ops int8 caem em caminho
        // lento no ORT. Ver plano_scheduler/BASELINE.md (Fase 3).
        std::string model_path       = "models/kokoro/model.onnx";
        std::string voice_path       = "models/kokoro/voices/pf_dora.bin";
        /// Blend de vozes: pares (path do .bin, peso). Se não-vazio, ignora
        /// voice_path — a voz final é a soma ponderada dos vetores de estilo
        /// (pesos normalizados). Truque padrão do Kokoro p/ criar timbres
        /// novos sem treinar nada, ex.: 60% pf_dora + 40% af_heart.
        std::vector<std::pair<std::string, float>> voice_blend;
        std::string vocab_path       = "models/kokoro/config.json"; // campo "vocab"
        std::string espeak_data_path = "models/kokoro/espeak-ng-data";
        std::string espeak_voice     = "pt-br";
        float speed                  = 1.0f;
        int   intra_threads          = 8;   // 5800X: 8 vs 4 ≈ -15% de RTF; LLM roda na GPU
        bool  verbose_timing         = true; // loga g2p/onnx/RTF por sentença
    };

    explicit KokoroTTS(Config cfg = {});
    ~KokoroTTS() override;

    // --- Contrato do TTSResident (lambdas de load/unload do scheduler) ---

    /// Carrega sessão ONNX + voice + vocab + espeak. Idempotente; thread-safe.
    bool load();

    /// Libera a sessão ONNX (o grosso da RAM). espeak/vocab ficam.
    bool unload();

    bool is_loaded() const;

    /// RAM aproximada da sessão (tamanho do modelo em disco). VRAM é 0.
    std::size_t resident_bytes() const;

    // --- Interface ITTS (streaming por sentença do AlyssaNet) ---
    void synthesizeAndPlay(const std::string& text) override;

    // --- Blocos expostos para teste/medição (test_kokoro) ---

    /// Texto → string de fonemas IPA (pós-processada estilo misaki).
    std::string phonemize(const std::string& text);

    /// Texto → waveform 24kHz mono. Vazio em falha.
    std::vector<float> synthesize(const std::string& text);

    static constexpr int SAMPLE_RATE = 24000;

private:
    struct EspeakApi; // ponteiros de função da libespeak-ng (dinâmica)

    bool ensure_loaded_locked();   // load com mtx_ já travado
    bool init_espeak_locked();
    bool load_vocab_locked();
    bool load_voice_locked();

    std::string phonemize_locked(const std::string& text);
    std::vector<int64_t> tokenize_locked(const std::string& phonemes) const;
    std::vector<float> run_model_locked(const std::vector<int64_t>& tokens);
    void play_audio(const std::vector<float>& samples);

    Config cfg_;

    mutable std::mutex mtx_;      // serializa load/unload/síntese
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string input_ids_name_, style_name_, speed_name_, output_name_;

    std::unordered_map<uint32_t, int64_t> vocab_; // codepoint IPA → token id
    std::vector<float> voice_;                    // 510 x 256 (style por nº de fonemas)
    std::size_t model_bytes_ = 0;

    EspeakApi* espeak_ = nullptr; // singleton do processo (ver KokoroTTS.cpp)
    bool pa_initialized_ = false;
};

#endif // KOKORO_TTS_HPP
