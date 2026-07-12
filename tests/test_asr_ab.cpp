// test_asr_ab.cpp — A/B de ASR pro sotaque do Deyvid: Whisper vs Gemma 4 E2B.
//
// Fala no microfone → o VAD corta o enunciado → o MESMO áudio passa pelo
// Whisper (turbo q8, in-process) e pelo Gemma E2B (subprocesso llama-mtmd-cli,
// receita validada em docs/gemma4-migration.md). Os dois resultados saem na
// tela lado a lado e viram uma linha de JSONL; o WAV de cada enunciado fica
// guardado (vira dataset de sotaque pra fine-tune depois).
//
//   test_asr_ab                     # modo microfone (Ctrl+C encerra)
//   test_asr_ab --wav arquivo.wav   # modo arquivo (sanidade sem microfone)
//   test_asr_ab --out meu.jsonl     # muda o arquivo de saída
//
// Rodar de build/Release (os paths default são relativos de lá).
// Cada linha do JSONL: {ts, wav, dur_s, whisper:{text,ms}, gemma:{text,ms}}.
// O ms do gemma INCLUI o load do modelo (~2.3s): o subprocesso nasce e morre
// a cada enunciado. Quente (integrado no daemon) fica <1s — medido à parte.

#include "voice/VoicePipeline.hpp"
#include "json.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::atomic<bool> g_stop{false};
void on_sigint(int) { g_stop = true; }

// ---------------------------------------------------------------------------
// WAV PCM16 mono — só o que o A/B precisa (writer 16kHz, reader qualquer taxa)
// ---------------------------------------------------------------------------

void write_wav16(const std::string& path, const std::vector<float>& samples, int rate) {
    std::ofstream f(path, std::ios::binary);
    auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);
    f.write("RIFF", 4); w32(36 + data_bytes); f.write("WAVE", 4);
    f.write("fmt ", 4); w32(16); w16(1); w16(1); w32(rate); w32(rate * 2); w16(2); w16(16);
    f.write("data", 4); w32(data_bytes);
    for (float s : samples) {
        float c = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
        int16_t v = static_cast<int16_t>(c * 32767.0f);
        f.write(reinterpret_cast<const char*>(&v), 2);
    }
}

/// Lê WAV PCM16 mono; devolve amostras float e a taxa. false = formato não suportado.
bool read_wav16(const std::string& path, std::vector<float>& out, int& rate) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    char riff[4]; uint32_t sz; char wave[4];
    f.read(riff, 4); f.read(reinterpret_cast<char*>(&sz), 4); f.read(wave, 4);
    if (std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE") return false;

    uint16_t channels = 0, bits = 0;
    rate = 0;
    while (f.good()) {
        char id[4]; uint32_t chunk_sz;
        f.read(id, 4); f.read(reinterpret_cast<char*>(&chunk_sz), 4);
        if (!f.good()) break;
        std::string cid(id, 4);
        if (cid == "fmt ") {
            uint16_t fmt; uint32_t brate; uint16_t align;
            f.read(reinterpret_cast<char*>(&fmt), 2);
            f.read(reinterpret_cast<char*>(&channels), 2);
            f.read(reinterpret_cast<char*>(&rate), 4);
            f.read(reinterpret_cast<char*>(&brate), 4);
            f.read(reinterpret_cast<char*>(&align), 2);
            f.read(reinterpret_cast<char*>(&bits), 2);
            f.seekg(chunk_sz - 16, std::ios::cur);
            if (fmt != 1 || channels != 1 || bits != 16) {
                std::cerr << "[asr_ab] WAV precisa ser PCM16 mono (canais=" << channels
                          << " bits=" << bits << ")" << std::endl;
                return false;
            }
        } else if (cid == "data") {
            out.resize(chunk_sz / 2);
            std::vector<int16_t> raw(out.size());
            f.read(reinterpret_cast<char*>(raw.data()), chunk_sz);
            for (size_t i = 0; i < raw.size(); ++i) out[i] = raw[i] / 32768.0f;
            return rate > 0;
        } else {
            f.seekg(chunk_sz + (chunk_sz & 1), std::ios::cur);
        }
    }
    return false;
}

/// Linear e suficiente pro Whisper (o mtmd-cli lê o arquivo original direto).
std::vector<float> resample_to_16k(const std::vector<float>& in, int rate) {
    if (rate == SAMPLE_RATE || in.empty()) return in;
    const double ratio = static_cast<double>(rate) / SAMPLE_RATE;
    std::vector<float> out(static_cast<size_t>(in.size() / ratio));
    for (size_t i = 0; i < out.size(); ++i) {
        const double pos = i * ratio;
        const size_t i0 = static_cast<size_t>(pos);
        const size_t i1 = std::min(i0 + 1, in.size() - 1);
        const float frac = static_cast<float>(pos - i0);
        out[i] = in[i0] * (1.0f - frac) + in[i1] * frac;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Gemma E2B via subprocesso (llama-mtmd-cli, receita de docs/gemma4-migration.md)
// ---------------------------------------------------------------------------

struct GemmaConfig {
    std::string cli    = "..\\..\\llama.cpp\\build_cuda\\bin\\Release\\llama-mtmd-cli.exe";
    std::string model  = "models/gemma-4-E2B_q4_0-it.gguf";
    std::string mmproj = "models/mmproj-gemma-4-E2B-it-Q8_0.gguf";
};

// Turn format REAL do Gemma 4 numa linha só (mesmo formato do
// ExpertBase::format_gemma4_prompt). Com `--chat-template gemma` (formato do
// Gemma 3) o modelo devolvia VAZIO em áudio de microfone — só o áudio
// sintético limpo da Kokoro sobrevivia ao template errado. Vai por variável
// de ambiente (o subprocesso herda) porque {% %} inline no cmd.exe é roleta.
constexpr const char* kGemma4Jinja =
    "{{ bos_token }}{% for m in messages %}<|turn>"
    "{{ \"model\" if m[\"role\"] == \"assistant\" else m[\"role\"] }}{{ \"\\n\" }}"
    "{{ m[\"content\"] | trim }}<turn|>{{ \"\\n\" }}{% endfor %}"
    "{% if add_generation_prompt %}<|turn>model{{ \"\\n\" }}{% endif %}";

void export_gemma4_template_env() {
#ifdef _WIN32
    _putenv_s("LLAMA_ARG_JINJA", "1");
    _putenv_s("LLAMA_ARG_CHAT_TEMPLATE", kGemma4Jinja);
#else
    setenv("LLAMA_ARG_JINJA", "1", 1);
    setenv("LLAMA_ARG_CHAT_TEMPLATE", kGemma4Jinja, 1);
#endif
}

std::string run_gemma_asr(const GemmaConfig& cfg, const std::string& wav_path, long long& ms_out) {
    // Template Gemma 4 vem por env (LLAMA_ARG_JINJA/CHAT_TEMPLATE — ver
    // export_gemma4_template_env). O Jinja de 16KB do GGUF fail-fasta o
    // minja (0xC0000409), então NUNCA rodar sem template explícito.
    // <__media__> no FIM: model card manda áudio depois do texto.
    // Sem aspas no exe: cmd.exe (via _popen) tropeça em comando que COMEÇA
    // com aspas, e nenhum path deste projeto tem espaço.
    std::string cmd = cfg.cli +
        " -m " + cfg.model +
        " --mmproj " + cfg.mmproj +
        " --audio " + wav_path +
        " --temp 0 -n 128 -fit off"
        " -p \"Transcribe the following speech segment in Portuguese into Portuguese text."
        " Only output the transcription, with no newlines. <__media__>\""
#ifdef _WIN32
        " 2>NUL";
#else
        " 2>/dev/null";
#endif

    auto t0 = std::chrono::steady_clock::now();
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) { ms_out = 0; return "[erro: não abriu o llama-mtmd-cli]"; }

    std::string raw;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) raw += buf;
    int rc = PCLOSE(pipe);
    ms_out = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    if (rc != 0) return "[erro: mtmd-cli saiu com código " + std::to_string(rc) + "]";

    // O stdout traz banner experimental + link além do texto gerado; fica só
    // o que parece transcrição. Heurística de ferramenta de teste, assumida.
    std::string text;
    std::istringstream lines(raw);
    std::string line;
    while (std::getline(lines, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        if (line.find("://") != std::string::npos) continue;
        if (line.rfind("WARN", 0) == 0 || line.find("experimental") != std::string::npos) continue;
        if (!text.empty()) text += " ";
        text += line;
    }
    return text;
}

// ---------------------------------------------------------------------------
// Um enunciado completo: WAV → dois ASRs → console + JSONL
// ---------------------------------------------------------------------------

struct ABContext {
    GemmaConfig gemma;
    std::string out_path = "asr_ab_results.jsonl";
    std::string wav_dir  = "asr_ab_wavs";
    std::string session;  // prefixo por sessão: sem ele a rodada seguinte sobrescrevia os WAVs da anterior
    int counter = 0;
};

void process_segment(ABContext& ctx, const std::vector<float>& audio16k,
                     const std::string& whisper_text, long long whisper_ms) {
    ctx.counter++;
    const double dur_s = audio16k.size() / static_cast<double>(SAMPLE_RATE);

    char wav_name[80];
    std::snprintf(wav_name, sizeof(wav_name), "%s_seg_%03d.wav", ctx.session.c_str(), ctx.counter);
    const std::string wav_path = ctx.wav_dir + "/" + wav_name;
    write_wav16(wav_path, audio16k, SAMPLE_RATE);

    std::cout << "\n[#" << ctx.counter << " | " << std::fixed;
    std::cout.precision(1);
    std::cout << dur_s << "s]  gemma pensando..." << std::endl;

    long long gemma_ms = 0;
    const std::string gemma_text = run_gemma_asr(ctx.gemma, wav_path, gemma_ms);

    std::cout << "  whisper (" << whisper_ms << "ms): "
              << (whisper_text.empty() ? "(vazio)" : whisper_text) << "\n";
    std::cout << "  gemma   (" << gemma_ms << "ms*): "
              << (gemma_text.empty() ? "(vazio)" : gemma_text) << "\n";
    std::cout << "  (*inclui ~2.3s de load do subprocesso)" << std::endl;

    json line = {
        {"ts", static_cast<long long>(std::time(nullptr))},
        {"wav", wav_path},
        {"dur_s", dur_s},
        {"whisper", {{"text", whisper_text}, {"ms", whisper_ms}}},
        {"gemma",   {{"text", gemma_text},   {"ms", gemma_ms}}},
    };
    std::ofstream out(ctx.out_path, std::ios::app);
    out << line.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string wav_file;
    ABContext ctx;
    std::string whisper_model = "models/ggml-large-v3-turbo-q8_0.bin";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--wav" && i + 1 < argc) wav_file = argv[++i];
        else if (arg == "--out" && i + 1 < argc) ctx.out_path = argv[++i];
        else if (arg == "--whisper-model" && i + 1 < argc) whisper_model = argv[++i];
        else if (arg == "--gemma" && i + 1 < argc) ctx.gemma.model = argv[++i];
        else if (arg == "--mmproj" && i + 1 < argc) ctx.gemma.mmproj = argv[++i];
        else if (arg == "--mtmd-cli" && i + 1 < argc) ctx.gemma.cli = argv[++i];
        else {
            std::cout << "uso: test_asr_ab [--wav arquivo.wav] [--out saida.jsonl]\n"
                         "                 [--whisper-model m] [--gemma m] [--mmproj m] [--mtmd-cli exe]\n";
            return 1;
        }
    }

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // acentos do pt-BR no console
#endif
    export_gemma4_template_env();
    fs::create_directories(ctx.wav_dir);
    ctx.session = std::to_string(std::time(nullptr) % 1000000);

    // Sessão nova = linha de meta no JSONL (separa rodadas na análise)
    {
        json meta = {
            {"type", "meta"},
            {"ts", static_cast<long long>(std::time(nullptr))},
            {"whisper_model", whisper_model},
            {"gemma_model", ctx.gemma.model},
            {"mmproj", ctx.gemma.mmproj},
        };
        std::ofstream out(ctx.out_path, std::ios::app);
        out << meta.dump() << "\n";
    }

    // ------------------------------------------------------------------
    // Modo arquivo: sanidade da ferramenta sem microfone
    // ------------------------------------------------------------------
    if (!wav_file.empty()) {
        std::vector<float> samples;
        int rate = 0;
        if (!read_wav16(wav_file, samples, rate)) {
            std::cerr << "[asr_ab] não consegui ler " << wav_file << std::endl;
            return 1;
        }
        std::cout << "[asr_ab] " << wav_file << " (" << rate << "Hz, "
                  << samples.size() / static_cast<double>(rate) << "s)" << std::endl;
        const std::vector<float> audio16k = resample_to_16k(samples, rate);

        // Whisper direto (sem VAD — o arquivo já é um enunciado), mesmo
        // process_segment do modo microfone.
        VoicePipeline::Options opts;
        VoicePipeline stt(whisper_model, opts, /*defer_model_load=*/false);
        long long whisper_ms = 0;
        const std::string whisper_text = stt.transcribe_buffer(audio16k, whisper_ms);
        process_segment(ctx, audio16k, whisper_text, whisper_ms);
        std::cout << "\n[asr_ab] resultado em " << ctx.out_path << std::endl;
        return 0;
    }

    // ------------------------------------------------------------------
    // Modo microfone: VAD corta, cada enunciado passa nos dois ASRs
    // ------------------------------------------------------------------
    std::signal(SIGINT, on_sigint);

    VoicePipeline::Options opts; // defaults: Silero, beam 5, prompt de sotaque
    VoicePipeline stt(whisper_model, opts, /*defer_model_load=*/false);

    stt.set_on_segment([&ctx](const std::vector<float>& audio,
                              const std::string& text, long long ms) {
        process_segment(ctx, audio, text, ms);
    });

    if (!stt.start()) {
        std::cerr << "[asr_ab] falha ao iniciar a captura (microfone?)" << std::endl;
        return 1;
    }

    std::cout << "\n=== A/B de ASR: Whisper turbo-q8 vs Gemma 4 E2B ===\n"
                 "Fala frases variadas (curtas, longas, gíria, nomes próprios,\n"
                 "números). Cada enunciado sai nos dois e vai pro JSONL.\n"
                 "Ctrl+C encerra. Resultados: " << ctx.out_path << "\n" << std::endl;

    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[asr_ab] encerrando..." << std::endl;
    stt.stop();
    std::cout << "[asr_ab] " << ctx.counter << " enunciados em " << ctx.out_path
              << " (WAVs em " << ctx.wav_dir << "/)" << std::endl;
    return 0;
}
