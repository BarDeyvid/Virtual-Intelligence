// test_gameplay_audio.cpp — aceite do plano B (voz direto no gameplayModel).
//
// Carrega SÓ o E2B + mmproj (sem o resto do cérebro), monta um estado de
// mundo fake com rótulos e injeta um WAV como [VOZ DO JOGADOR] <__media__>.
// Passa se a saída casa com a grammar do gameplay ([AÇÃO] verbo ...).
//
//   test_gameplay_audio --wav comando.wav
//   test_gameplay_audio --wav comando.wav --expect mover   # verbo esperado
//
// Rodar de build/Release. Áudio de teste sem microfone:
//   test_kokoro --say "vai até a árvore" --out cmd_arvore.wav

#include "AlyssaCore.hpp"
#include "ExpertBase.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace {

// WAV PCM16 mono → float + resample linear pra 16kHz (mesmos helpers do
// test_asr_ab; duplicado conscientemente — são 40 linhas de código de teste)
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
            if (fmt != 1 || channels != 1 || bits != 16) return false;
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

std::vector<float> resample_to_16k(const std::vector<float>& in, int rate) {
    if (rate == 16000 || in.empty()) return in;
    const double ratio = static_cast<double>(rate) / 16000.0;
    std::vector<float> out(static_cast<size_t>(in.size() / ratio));
    for (size_t i = 0; i < out.size(); ++i) {
        const double pos = i * ratio;
        const size_t i0 = static_cast<size_t>(pos);
        const size_t i1 = (i0 + 1 < in.size()) ? i0 + 1 : in.size() - 1;
        const float frac = static_cast<float>(pos - i0);
        out[i] = in[i0] * (1.0f - frac) + in[i1] * frac;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string wav_path;
    std::string expect_verb; // opcional: verbo que a ação DEVE usar
    std::string gemma_path  = "models/gemma-4-E2B_q4_0-it.gguf";
    std::string mmproj_path = "models/mmproj-gemma-4-E2B-it-Q8_0.gguf";

    int repeat = 1;
    bool no_grammar = false; // experimento: E2B segura o formato sem grammar?
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--wav" && i + 1 < argc) wav_path = argv[++i];
        else if (arg == "--expect" && i + 1 < argc) expect_verb = argv[++i];
        else if (arg == "--gemma" && i + 1 < argc) gemma_path = argv[++i];
        else if (arg == "--mmproj" && i + 1 < argc) mmproj_path = argv[++i];
        else if (arg == "--repeat" && i + 1 < argc) repeat = std::atoi(argv[++i]);
        else if (arg == "--no-grammar") no_grammar = true;
        else {
            std::cout << "uso: test_gameplay_audio --wav arquivo.wav [--expect verbo] [--repeat N]\n";
            return 2;
        }
    }
    if (repeat < 1) repeat = 1;
    if (wav_path.empty()) {
        std::cout << "uso: test_gameplay_audio --wav arquivo.wav [--expect verbo]\n";
        return 2;
    }

    // 1. Áudio
    std::vector<float> samples;
    int rate = 0;
    if (!read_wav16(wav_path, samples, rate)) {
        std::cerr << "FALHA: não li " << wav_path << " (PCM16 mono?)" << std::endl;
        return 2;
    }
    const std::vector<float> audio = resample_to_16k(samples, rate);
    std::cout << "[gameplay_audio] " << wav_path << " (" << rate << "Hz → 16k, "
              << audio.size() / 16000.0 << "s)" << std::endl;

    // 2. Config do gameplayModel (system/role/grammar) direto do JSON — a
    //    mesma que o CoreIntegration usa.
    AllModelConfigs configs = load_config();
    const SimpleModelConfig* gp = nullptr;
    for (const auto& c : configs) if (c.id == "gameplayModel") gp = &c;
    if (!gp) {
        std::cerr << "FALHA: gameplayModel ausente do ConfigsLLM.json" << std::endl;
        return 2;
    }

    // 3. Só o E2B + encoder (sem comitê, sem persona, sem embedder)
    alyssa_core::AlyssaCore core(gp->model_path, 4096);
    if (!core.init_audio(mmproj_path)) {
        std::cerr << "FALHA: mmproj não carregou" << std::endl;
        return 2;
    }

    // 4. Estado de mundo fake, mesmo formato do MinecraftSession::build_prompt
    std::string world =
        "[ESTADO DO JOGO]\n"
        "Posição: (10, 64, -3)\n"
        "Vida: 18/20  Fome: 12/20\n"
        "Hora do jogo: dia\n"
        "Inventário: 3x oak_log, 1x wooden_pickaxe, 2x bread\n"
        "Blocos próximos (use o rótulo para mover/minerar/colocar): "
        "B1=oak_log(12,65,-2), B2=stone(9,63,-3), B3=crafting_table(11,64,-5)\n"
        "Entidades próximas (use o rótulo para mover/atacar): "
        "E1=zombie a 6.0 blocos (14,64,-8), E2=rabbit a 3.0 blocos (8,64,-1)\n"
        "[VOZ DO JOGADOR] (obedeça se for um comando) <__media__>\n";

    std::string user_text = gp->role_instruction.empty()
        ? world : "[ROLE]: " + gp->role_instruction + "\n" + world;

    std::vector<llama_chat_message> msgs;
    msgs.push_back({"system", gp->system_prompt.c_str()});
    msgs.push_back({"user", user_text.c_str()});
    const std::string prompt = alyssa_experts::ExpertBase::format_gemma4_prompt(msgs);

    // 5. Gera e valida contra o mesmo regex do ActionExecutor. Com --repeat,
    //    a última rodada é o número QUENTE (a 1ª paga warmup do encoder).
    SimpleModelParameters run_params = gp->params;
    if (no_grammar) run_params.grammar.clear();

    std::string signal;
    for (int r = 0; r < repeat; ++r) {
        core.clear_kv();
        auto t0 = std::chrono::steady_clock::now();
        signal = core.generate_with_audio(prompt, audio, run_params, nullptr);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        std::cout << "[gameplay_audio] rodada " << (r + 1) << " (" << ms
                  << "ms): " << signal << std::endl;
    }

    static const std::regex pattern(
        R"(\[AÇÃO\]\s*(\w+)\s*(.*?)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[CONTEXTO\]\s*(.+))");
    std::smatch m;
    if (!std::regex_search(signal, m, pattern)) {
        std::cout << "FALHA: sinal fora da grammar do gameplay" << std::endl;
        return 1;
    }
    const std::string verb = m[1];
    std::cout << "[gameplay_audio] verbo=" << verb << " args='" << m[2] << "'" << std::endl;

    if (!expect_verb.empty() && verb != expect_verb) {
        std::cout << "FALHA: esperava verbo '" << expect_verb << "', veio '" << verb << "'" << std::endl;
        return 1;
    }
    std::cout << "OK" << std::endl;
    return 0;
}
