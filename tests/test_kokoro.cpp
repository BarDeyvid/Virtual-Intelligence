// =============================================================================
// test_kokoro.cpp — Fase 3 do plano_scheduler: mede o custo REAL do Kokoro TTS
// local antes de decidir tier (TIER2_JIT vs TIER1_HOT).
//
// Mede:
//   - tempo de load da sessão ONNX (frio e quente) + delta de RAM
//   - delta de VRAM via nvidia-smi (esperado: 0 — ORT do vcpkg é CPU-only)
//   - fonemização pt-br (sanidade do G2P espeak-ng)
//   - latência texto→waveform por sentença e RTF (CPU, Ryzen 7 5800X)
//
// Rodar de dentro de build/Release (precisa de models/kokoro e libespeak-ng.dll).
// Flags:
//   --play             reproduz as sentenças do benchmark
//   --model <path>     modelo ONNX (default: fp32)
//   --threads <n>      intra-op threads do ORT
//   --audition         toca e salva .wav de blends candidatos de voz
//                      (feminina/jovem) para escolher de ouvido
//   --voice <spec>     blend custom, ex.: --voice "pf_dora:0.6,af_heart:0.4"
//                      (nomes sem '/' viram models/kokoro/voices/<nome>.bin)
// =============================================================================

#include "voice/KokoroTTS.hpp"

#include <portaudio.h>

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <sys/resource.h>
#define POPEN popen
#define PCLOSE pclose
#endif

namespace {

using clock_t_ = std::chrono::steady_clock;

long long ms_since(clock_t_::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock_t_::now() - t0).count();
}

// Uso de VRAM global da GPU 0, em MiB. -1 em falha (mesmo esquema do vram_baseline).
long vram_used_mib() {
    FILE* pipe = POPEN("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits", "r");
    if (!pipe) return -1;
    char buf[64] = {0};
    const bool ok = fgets(buf, sizeof(buf), pipe) != nullptr;
    PCLOSE(pipe);
    return ok ? std::strtol(buf, nullptr, 10) : -1;
}

// Working set do processo em MiB (RAM de verdade, não VRAM).
long ram_used_mib() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<long>(pmc.WorkingSetSize >> 20);
    return -1;
#else
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return static_cast<long>(ru.ru_maxrss >> 10);
#endif
}

using VoiceSpec = std::vector<std::pair<std::string, float>>;

// "pf_dora:0.6,af_heart:0.4" → [(models/kokoro/voices/pf_dora.bin, 0.6), ...]
VoiceSpec parse_voice_spec(const std::string& spec) {
    VoiceSpec out;
    std::size_t pos = 0;
    while (pos < spec.size()) {
        std::size_t comma = spec.find(',', pos);
        if (comma == std::string::npos) comma = spec.size();
        std::string item = spec.substr(pos, comma - pos);
        pos = comma + 1;
        const std::size_t colon = item.find(':');
        std::string name = colon == std::string::npos ? item : item.substr(0, colon);
        const float w = colon == std::string::npos ? 1.0f : std::stof(item.substr(colon + 1));
        if (name.find('/') == std::string::npos && name.find(".bin") == std::string::npos)
            name = "models/kokoro/voices/" + name + ".bin";
        out.emplace_back(name, w);
    }
    return out;
}

// WAV PCM16 mono 24kHz — p/ reouvir as candidatas sem rodar o teste de novo.
bool write_wav(const std::string& path, const std::vector<float>& samples, int rate) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);
    const uint32_t chunk = 36 + data_bytes;
    const uint16_t fmt16[] = {1, 1};                       // PCM, mono
    const uint32_t byte_rate = static_cast<uint32_t>(rate) * 2;
    const uint16_t align_bits[] = {2, 16};                 // block align, bits
    f.write("RIFF", 4).write(reinterpret_cast<const char*>(&chunk), 4).write("WAVE", 4);
    f.write("fmt ", 4);
    const uint32_t fmt_size = 16;
    f.write(reinterpret_cast<const char*>(&fmt_size), 4);
    f.write(reinterpret_cast<const char*>(fmt16), 4);
    const uint32_t r = static_cast<uint32_t>(rate);
    f.write(reinterpret_cast<const char*>(&r), 4);
    f.write(reinterpret_cast<const char*>(&byte_rate), 4);
    f.write(reinterpret_cast<const char*>(align_bits), 4);
    f.write("data", 4).write(reinterpret_cast<const char*>(&data_bytes), 4);
    for (float s : samples) {
        const float c = s < -1.f ? -1.f : (s > 1.f ? 1.f : s);
        const int16_t v = static_cast<int16_t>(c * 32767.f);
        f.write(reinterpret_cast<const char*>(&v), 2);
    }
    return f.good();
}

void play_samples(const std::vector<float>& samples) {
    if (samples.empty()) return;
    if (Pa_Initialize() != paNoError) return;
    PaStream* stream = nullptr;
    if (Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, KokoroTTS::SAMPLE_RATE, 256,
                             nullptr, nullptr) == paNoError) {
        Pa_StartStream(stream);
        Pa_WriteStream(stream, samples.data(), static_cast<unsigned long>(samples.size()));
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    Pa_Terminate(); // ref-contado: não derruba o PA interno do KokoroTTS
}

// Toca e salva blends candidatos (direção: feminina, mais jovem que pf_dora).
// Truque: âncora pt-br (pf_dora) p/ segurar o sotaque + timbre de vozes
// inglesas/italianas de qualidade alta (af_heart A, af_bella A-, af_sky,
// if_sara) p/ mudar a cor da voz.
int run_audition(KokoroTTS::Config base_cfg) {
    const std::vector<std::pair<std::string, std::string>> candidates = {
        {"1_pf_dora_pura",        "pf_dora:1.0"},
        {"2_dora60_heart40",      "pf_dora:0.6,af_heart:0.4"},
        {"3_dora50_bella50",      "pf_dora:0.5,af_bella:0.5"},
        {"4_dora60_sky40",        "pf_dora:0.6,af_sky:0.4"},
        {"5_dora50_sara50",       "pf_dora:0.5,if_sara:0.5"},
        {"6_dora30_heart70",      "pf_dora:0.3,af_heart:0.7"},
        {"7_dora40_bella30_sky30","pf_dora:0.4,af_bella:0.3,af_sky:0.3"},
    };
    const std::vector<std::string> lines = {
        "Oi! Eu sou a Alyssa. Que bom te ver por aqui!",
        "Hoje eu aprendi uma coisa nova. Quer que eu te conte?",
    };

    std::cout << "=== Audição de vozes (" << candidates.size()
              << " candidatas) — cada uma toca e vira um .wav ===\n" << std::endl;

    for (const auto& [label, spec] : candidates) {
        KokoroTTS::Config cfg = base_cfg;
        cfg.verbose_timing = false;
        cfg.voice_blend = parse_voice_spec(spec);
        KokoroTTS tts(cfg);
        if (!tts.load()) {
            std::cerr << "  [" << label << "] FALHOU no load — pulando" << std::endl;
            continue;
        }
        std::vector<float> all;
        for (const auto& l : lines) {
            const std::vector<float> wav = tts.synthesize(l);
            all.insert(all.end(), wav.begin(), wav.end());
            all.insert(all.end(), KokoroTTS::SAMPLE_RATE / 2, 0.0f); // 0.5s de pausa
        }
        const std::string out = "audition_" + label + ".wav";
        write_wav(out, all, KokoroTTS::SAMPLE_RATE);
        std::cout << ">> " << label << "  (" << spec << ")  →  " << out << std::endl;
        play_samples(all);
    }
    std::cout << "\nOs .wav ficaram no diretório atual — dá pra reouvir sem rodar o teste."
              << "\nP/ testar um blend custom: test_kokoro --voice \"pf_dora:0.5,af_heart:0.5\" --play"
              << std::endl;
    return 0;
}

} // namespace

#ifdef _WIN32
#pragma comment(lib, "psapi.lib")
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    bool play = false;
    bool audition = false;
    KokoroTTS::Config cfg;
    cfg.verbose_timing = false; // o teste imprime as métricas por conta própria
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--play") play = true;
        else if (arg == "--audition") audition = true;
        else if (arg == "--model" && i + 1 < argc) cfg.model_path = argv[++i];
        else if (arg == "--threads" && i + 1 < argc) cfg.intra_threads = std::atoi(argv[++i]);
        else if (arg == "--voice" && i + 1 < argc) cfg.voice_blend = parse_voice_spec(argv[++i]);
    }

    if (audition) return run_audition(cfg);

    if (!cfg.voice_blend.empty() && play) {
        // Modo blend custom: só sintetiza, toca e salva — sem benchmark.
        KokoroTTS tts(cfg);
        if (!tts.load()) return 1;
        const std::vector<float> wav =
            tts.synthesize("Oi! Eu sou a Alyssa. Que bom te ver por aqui! "
                           "Hoje eu aprendi uma coisa nova. Quer que eu te conte?");
        write_wav("audition_custom.wav", wav, KokoroTTS::SAMPLE_RATE);
        std::cout << "blend custom → audition_custom.wav" << std::endl;
        play_samples(wav);
        return 0;
    }

    std::cout << "=== test_kokoro — modelo: " << cfg.model_path << " ===\n" << std::endl;

    KokoroTTS tts(cfg);

    const long vram0 = vram_used_mib();
    const long ram0 = ram_used_mib();

    // --- Load frio (1º do processo: inclui init do espeak + vocab + voice) ---
    auto t0 = clock_t_::now();
    if (!tts.load()) {
        std::cerr << "FALHA no load — abortando." << std::endl;
        return 1;
    }
    const long long load_frio_ms = ms_since(t0);
    const long vram1 = vram_used_mib();
    const long ram1 = ram_used_mib();

    std::cout << "Load frio:   " << load_frio_ms << "ms | RAM +"
              << (ram1 - ram0) << " MiB | VRAM "
              << (vram0 >= 0 && vram1 >= 0 ? std::to_string(vram1 - vram0) + " MiB"
                                           : std::string("n/d")) << std::endl;

    // --- Sanidade do G2P pt-br ---
    const std::string sample = "Olá! Eu sou a Alyssa, tudo bem com você?";
    std::cout << "\nG2P: \"" << sample << "\"\n  → " << tts.phonemize(sample) << "\n" << std::endl;

    // --- Latência de síntese por sentença (o número que importa p/ o turno) ---
    const std::vector<std::string> sentences = {
        "Olá! Eu sou a Alyssa.",
        "O tempo hoje está ótimo para programar alguma coisa nova.",
        "Essa é uma sentença um pouco mais longa, para medir como o custo de "
        "inferência cresce com o tamanho do texto que eu preciso falar.",
    };
    double first_latency_ms = -1;
    for (const auto& s : sentences) {
        t0 = clock_t_::now();
        const std::vector<float> wav = tts.synthesize(s);
        const long long lat = ms_since(t0);
        if (first_latency_ms < 0) first_latency_ms = static_cast<double>(lat);
        const double audio_s = static_cast<double>(wav.size()) / KokoroTTS::SAMPLE_RATE;
        std::cout << "Síntese: " << lat << "ms → " << audio_s << "s de fala (RTF="
                  << (audio_s > 0 ? lat / 1000.0 / audio_s : 0)
                  << ") [" << s.substr(0, 40) << "...]" << std::endl;
        if (play && !wav.empty()) tts.synthesizeAndPlay(s);
    }

    // --- Ciclo JIT em regime: unload + load quente ---
    t0 = clock_t_::now();
    tts.unload();
    const long long unload_ms = ms_since(t0);
    const long ram2 = ram_used_mib();

    t0 = clock_t_::now();
    if (!tts.load()) {
        std::cerr << "FALHA no reload quente." << std::endl;
        return 1;
    }
    const long long load_quente_ms = ms_since(t0);
    const long ram3 = ram_used_mib();
    const long vram2 = vram_used_mib();

    std::cout << "\nUnload:      " << unload_ms << "ms | RAM " << (ram2 - ram1) << " MiB" << std::endl;
    std::cout << "Load quente: " << load_quente_ms << "ms | RAM +" << (ram3 - ram2)
              << " MiB (ciclo JIT em regime)" << std::endl;

    // --- Resumo p/ BASELINE.md ---
    std::cout << "\n--- linha p/ plano_scheduler/BASELINE.md ---" << std::endl;
    std::cout << "| Kokoro-82M (" << cfg.model_path << ") | VRAM "
              << (vram0 >= 0 ? std::to_string(vram2 - vram0) : std::string("n/d"))
              << " MiB | load frio " << load_frio_ms << "ms / quente " << load_quente_ms
              << "ms | unload " << unload_ms << "ms | RAM ~" << (ram3 - ram2)
              << " MiB | 1ª sentença " << first_latency_ms << "ms |" << std::endl;

    return 0;
}
