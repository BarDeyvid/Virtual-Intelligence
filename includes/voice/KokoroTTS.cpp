#include "voice/KokoroTTS.hpp"

#include "json.hpp"

#include <onnxruntime/onnxruntime_cxx_api.h>
#include <portaudio.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

// =============================================================================
// espeak-ng via carga dinâmica — só as 3 funções que o G2P precisa.
// Constantes de speak_lib.h (estáveis desde o espeak original):
//   AUDIO_OUTPUT_RETRIEVAL=1, espeakINITIALIZE_DONT_EXIT=0x8000,
//   espeakCHARS_UTF8=1, espeakPHONEMES_IPA=0x02, espeakPHONEMES_TIE=0x80.
// =============================================================================
struct KokoroTTS::EspeakApi {
    using Initialize_t     = int (*)(int output, int buflength, const char* path, int options);
    using SetVoiceByName_t = int (*)(const char* name);
    using TextToPhonemes_t = const char* (*)(const void** textptr, int textmode, int phonememode);

    Initialize_t     Initialize     = nullptr;
    SetVoiceByName_t SetVoiceByName = nullptr;
    TextToPhonemes_t TextToPhonemes = nullptr;
    void* handle = nullptr;
    bool initialized = false;

    bool open() {
#ifdef _WIN32
        HMODULE h = LoadLibraryA("libespeak-ng.dll");
        if (!h) return false;
        handle = h;
        Initialize     = reinterpret_cast<Initialize_t>(GetProcAddress(h, "espeak_Initialize"));
        SetVoiceByName = reinterpret_cast<SetVoiceByName_t>(GetProcAddress(h, "espeak_SetVoiceByName"));
        TextToPhonemes = reinterpret_cast<TextToPhonemes_t>(GetProcAddress(h, "espeak_TextToPhonemes"));
#else
        void* h = dlopen("libespeak-ng.so.1", RTLD_NOW);
        if (!h) h = dlopen("libespeak-ng.so", RTLD_NOW);
        if (!h) return false;
        handle = h;
        Initialize     = reinterpret_cast<Initialize_t>(dlsym(h, "espeak_Initialize"));
        SetVoiceByName = reinterpret_cast<SetVoiceByName_t>(dlsym(h, "espeak_SetVoiceByName"));
        TextToPhonemes = reinterpret_cast<TextToPhonemes_t>(dlsym(h, "espeak_TextToPhonemes"));
#endif
        return Initialize && SetVoiceByName && TextToPhonemes;
    }
};

// =============================================================================
// Helpers UTF-8 (vocab do Kokoro é indexado por codepoint)
// =============================================================================
namespace {

std::vector<uint32_t> utf8_decode(const std::string& s) {
    std::vector<uint32_t> cps;
    cps.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = s[i];
        uint32_t cp = 0;
        int len = 1;
        if (c < 0x80) { cp = c; }
        else if ((c >> 5) == 0x6 && i + 1 < s.size()) { cp = ((c & 0x1F) << 6) | (s[i+1] & 0x3F); len = 2; }
        else if ((c >> 4) == 0xE && i + 2 < s.size()) { cp = ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F); len = 3; }
        else if ((c >> 3) == 0x1E && i + 3 < s.size()) { cp = ((c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F); len = 4; }
        else { i++; continue; } // byte inválido: pula
        cps.push_back(cp);
        i += len;
    }
    return cps;
}

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

// Marcadores de troca de língua do espeak, ex. "(en)" — o phonemizer remove
// com language_switch='remove-flags'; fazemos o mesmo.
void strip_lang_flags(std::string& s) {
    std::size_t open;
    while ((open = s.find('(')) != std::string::npos) {
        const std::size_t close = s.find(')', open);
        if (close == std::string::npos) break;
        bool flag = close > open + 1;
        for (std::size_t i = open + 1; i < close && flag; ++i) {
            const char c = s[i];
            flag = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
        }
        if (flag) s.erase(open, close - open + 1);
        else break; // parênteses "de verdade" não aparecem na saída do espeak
    }
}

// Pós-processo do misaki (EspeakG2P, hexgrad/misaki): difongos/africadas
// com tie '^' viram os símbolos de 1 codepoint do vocab do Kokoro.
const std::pair<const char*, const char*> kE2M[] = {
    {"a^ɪ", "I"}, {"a^ʊ", "W"},
    {"d^z", "ʣ"}, {"d^ʒ", "ʤ"},
    {"e^ɪ", "A"},
    {"o^ʊ", "O"}, {"ə^ʊ", "Q"},
    {"s^s", "S"},
    {"t^s", "ʦ"}, {"t^ʃ", "ʧ"},
    {"ɔ^ɪ", "Y"},
};

// Limpeza de texto vindo do LLM: markdown/emoji/asteriscos não devem chegar
// no espeak. Mantém letras, pontuação falável e acentos latinos.
std::string clean_for_speech(const std::string& text) {
    const std::vector<uint32_t> cps = utf8_decode(text);
    std::string out;
    out.reserve(text.size());
    bool last_space = true;
    for (uint32_t cp : cps) {
        bool keep =
            (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
            (cp >= '0' && cp <= '9') ||
            (cp >= 0x00C0 && cp <= 0x024F) ||      // latim acentuado (pt-br)
            cp == ' ' || cp == ',' || cp == '.' || cp == ';' || cp == ':' ||
            cp == '!' || cp == '?' || cp == '\'' || cp == '-' || cp == '%' ||
            cp == 0x2026;                           // …
        if (cp == '\n' || cp == '\t' || cp == '*' || cp == '"' || cp == '#') {
            cp = ' ';
            keep = true;
        }
        if (!keep) continue;
        if (cp == ' ') {
            if (last_space) continue;
            last_space = true;
            out += ' ';
            continue;
        }
        last_space = false;
        // re-encode UTF-8
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// O onnxruntime do vcpkg é buildado contra o onnx.dll compartilhado e os dois
// registram os mesmos op schemas → o init do ORT cospe CENTENAS de linhas
// "Schema error: ... already registered" no stderr (cosmético, mas inunda o
// console do CLI). Silencia o fd 2 só durante o init. Trade-off consciente:
// logs concorrentes de llama/whisper nesse intervalo (~1.5s no boot) também
// somem — aceitável, o load do TTS roda uma vez.
class StderrSilencer {
public:
    StderrSilencer() {
        std::fflush(stderr);
#ifdef _WIN32
        saved_ = _dup(2);
        FILE* f = nullptr;
        if (freopen_s(&f, "NUL", "w", stderr) != 0) saved_ = -1;
#else
        saved_ = dup(2);
        if (!std::freopen("/dev/null", "w", stderr)) saved_ = -1;
#endif
    }
    ~StderrSilencer() {
        if (saved_ < 0) return;
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved_, 2);
        _close(saved_);
#else
        dup2(saved_, 2);
        close(saved_);
#endif
    }
private:
    int saved_ = -1;
};

} // namespace

// =============================================================================
// Ciclo de vida
// =============================================================================

KokoroTTS::KokoroTTS(Config cfg) : cfg_(std::move(cfg)) {
    std::cout << "Inicializando KokoroTTS (modelo: " << cfg_.model_path
              << ", voz: " << cfg_.voice_path << ")" << std::endl;
}

KokoroTTS::~KokoroTTS() {
    unload();
    if (pa_initialized_) Pa_Terminate();
    // espeak fica inicializado até o fim do processo (ver header).
}

bool KokoroTTS::is_loaded() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return session_ != nullptr;
}

std::size_t KokoroTTS::resident_bytes() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return session_ ? model_bytes_ : 0;
}

bool KokoroTTS::load() {
    std::lock_guard<std::mutex> lk(mtx_);
    return ensure_loaded_locked();
}

bool KokoroTTS::unload() {
    std::lock_guard<std::mutex> lk(mtx_);
    session_.reset(); // o grosso da RAM (pesos + buffers da sessão ONNX)
    return true;
}

bool KokoroTTS::ensure_loaded_locked() {
    if (session_) return true;

    if (!init_espeak_locked() || !load_vocab_locked() || !load_voice_locked())
        return false;

    try {
        StderrSilencer mute; // spam de "Schema error" do onnx/ORT (ver acima)
        if (!env_) {
            env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "kokoro");
        }
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(cfg_.intra_threads);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const std::filesystem::path model_path(cfg_.model_path);
        if (!std::filesystem::exists(model_path)) {
            std::cerr << "[kokoro] modelo não encontrado: " << cfg_.model_path << std::endl;
            return false;
        }
        model_bytes_ = static_cast<std::size_t>(std::filesystem::file_size(model_path));
        session_ = std::make_unique<Ort::Session>(*env_, model_path.native().c_str(), opts);

        // Nomes de I/O variam entre exports (input_ids/tokens): resolve por nome.
        Ort::AllocatorWithDefaultOptions alloc;
        input_ids_name_.clear(); style_name_.clear(); speed_name_.clear();
        for (std::size_t i = 0; i < session_->GetInputCount(); ++i) {
            const std::string name = session_->GetInputNameAllocated(i, alloc).get();
            if (name.find("style") != std::string::npos)      style_name_ = name;
            else if (name.find("speed") != std::string::npos) speed_name_ = name;
            else                                              input_ids_name_ = name;
        }
        output_name_ = session_->GetOutputNameAllocated(0, alloc).get();
        if (input_ids_name_.empty() || style_name_.empty() || speed_name_.empty()) {
            std::cerr << "[kokoro] inputs inesperados no modelo ONNX" << std::endl;
            session_.reset();
            return false;
        }
    } catch (const Ort::Exception& e) {
        std::cerr << "[kokoro] falha ao criar sessão ONNX: " << e.what() << std::endl;
        session_.reset();
        return false;
    }

    if (!pa_initialized_) {
        pa_initialized_ = (Pa_Initialize() == paNoError);
        if (!pa_initialized_)
            std::cerr << "[kokoro] AVISO: PortAudio não inicializou (síntese ok, playback não)" << std::endl;
    }
    return true;
}

namespace {
// espeak-ng tem estado global e NÃO tolera espeak_Initialize repetido no
// mesmo processo (corrompe as tabelas de fonemas → "Invalid instruction N
// for phoneme ..." e crash). Então: UM singleton por processo, inicializado
// uma única vez, e um mutex global serializando qualquer chamada ao espeak
// (a lib também não é thread-safe entre instâncias de KokoroTTS).
std::mutex g_espeak_mtx;
} // namespace

bool KokoroTTS::init_espeak_locked() {
    static EspeakApi g_api; // singleton do processo
    std::lock_guard<std::mutex> lk(g_espeak_mtx);
    espeak_ = &g_api;
    if (g_api.initialized) return true;

    if (!g_api.handle && !g_api.open()) {
        std::cerr << "[kokoro] libespeak-ng não encontrada (esperada junto do executável)" << std::endl;
        espeak_ = nullptr;
        return false;
    }
    // AUDIO_OUTPUT_RETRIEVAL=1 (não vamos sintetizar áudio pelo espeak),
    // espeakINITIALIZE_DONT_EXIT=0x8000 (não derruba o processo em erro).
    const int rate = g_api.Initialize(1, 0, cfg_.espeak_data_path.c_str(), 0x8000);
    if (rate <= 0) {
        std::cerr << "[kokoro] espeak_Initialize falhou (espeak-ng-data em '"
                  << cfg_.espeak_data_path << "'?)" << std::endl;
        return false;
    }
    if (g_api.SetVoiceByName(cfg_.espeak_voice.c_str()) != 0) {
        std::cerr << "[kokoro] espeak_SetVoiceByName('" << cfg_.espeak_voice << "') falhou" << std::endl;
        return false;
    }
    g_api.initialized = true;
    return true;
}

bool KokoroTTS::load_vocab_locked() {
    if (!vocab_.empty()) return true;
    std::ifstream f(cfg_.vocab_path);
    if (!f.is_open()) {
        std::cerr << "[kokoro] vocab não encontrado: " << cfg_.vocab_path << std::endl;
        return false;
    }
    try {
        json j;
        f >> j;
        for (const auto& [key, val] : j.at("vocab").items()) {
            const auto cps = utf8_decode(key);
            if (cps.size() == 1) vocab_[cps[0]] = val.get<int64_t>();
        }
    } catch (const json::exception& e) {
        std::cerr << "[kokoro] erro lendo vocab: " << e.what() << std::endl;
        return false;
    }
    return !vocab_.empty();
}

namespace {
// Formato v1.0: 510 linhas x 256 floats (style indexado pelo nº de fonemas)
bool read_voice_file(const std::string& path, std::vector<float>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        std::cerr << "[kokoro] voz não encontrada: " << path << std::endl;
        return false;
    }
    const std::streamsize bytes = f.tellg();
    if (bytes != 510LL * 256 * 4) {
        std::cerr << "[kokoro] voz com tamanho inesperado (" << bytes
                  << " bytes, esperado " << 510 * 256 * 4 << "): " << path << std::endl;
        return false;
    }
    out.resize(510 * 256);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()), bytes);
    return f.good();
}
} // namespace

bool KokoroTTS::load_voice_locked() {
    if (!voice_.empty()) return true;

    // Sem blend: caminho único. Com blend: soma ponderada dos vetores de
    // estilo, pesos normalizados para somar 1 (interpolar fora do simplex
    // degrada rápido).
    std::vector<std::pair<std::string, float>> spec = cfg_.voice_blend;
    if (spec.empty()) spec.emplace_back(cfg_.voice_path, 1.0f);

    float total = 0.0f;
    for (const auto& [path, w] : spec) total += w;
    if (total <= 0.0f) {
        std::cerr << "[kokoro] blend de vozes com pesos inválidos" << std::endl;
        return false;
    }

    voice_.assign(510 * 256, 0.0f);
    std::vector<float> tmp;
    for (const auto& [path, w] : spec) {
        if (!read_voice_file(path, tmp)) {
            voice_.clear();
            return false;
        }
        const float k = w / total;
        for (std::size_t i = 0; i < voice_.size(); ++i) voice_[i] += k * tmp[i];
    }
    return true;
}

// =============================================================================
// G2P: texto → IPA (espeak) → pós-processo misaki
// =============================================================================

std::string KokoroTTS::phonemize(const std::string& text) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!init_espeak_locked() || !load_vocab_locked()) return "";
    return phonemize_locked(text);
}

std::string KokoroTTS::phonemize_locked(const std::string& text) {
    // espeak_TextToPhonemes descarta pontuação (processa por cláusula), mas o
    // Kokoro usa pontuação para prosódia. Então: split nos delimitadores aqui,
    // fonemiza cada pedaço e recoloca o delimitador — mesmo efeito do
    // preserve_punctuation=True do phonemizer.
    // IPA + tie '^' (igual ao misaki): 0x02 | 0x80 | ('^' << 8)
    constexpr int kPhonemeMode = 0x02 | 0x80 | ('^' << 8);
    constexpr int kCharsUtf8 = 1;

    std::lock_guard<std::mutex> espeak_lk(g_espeak_mtx); // espeak é global/não-thread-safe

    std::string result;
    std::string chunk;
    auto flush_chunk = [&](const std::string& punct) {
        if (!chunk.empty()) {
            const void* ptr = chunk.c_str();
            bool first = true;
            while (ptr) {
                const char* ph = espeak_->TextToPhonemes(&ptr, kCharsUtf8, kPhonemeMode);
                if (!ph) break;
                if (!first && ph[0] != '\0') result += ' ';
                result += ph;
                first = false;
            }
            chunk.clear();
        }
        if (!punct.empty()) {
            result += punct;
            result += ' ';
        }
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ',' || c == ';' || c == ':' || c == '.' || c == '!' || c == '?') {
            flush_chunk(std::string(1, c));
        } else if ((unsigned char)c == 0xE2 && i + 2 < text.size() &&
                   (unsigned char)text[i+1] == 0x80 && (unsigned char)text[i+2] == 0xA6) {
            flush_chunk("…"); // U+2026
            i += 2;
        } else {
            chunk += c;
        }
    }
    flush_chunk("");

    strip_lang_flags(result);
    for (const auto& [from, to] : kE2M) replace_all(result, from, to);
    replace_all(result, "^", "");
    replace_all(result, "-", "");
    replace_all(result, "\n", " ");
    while (!result.empty() && result.back() == ' ') result.pop_back();
    std::size_t start = result.find_first_not_of(' ');
    return start == std::string::npos ? "" : result.substr(start);
}

std::vector<int64_t> KokoroTTS::tokenize_locked(const std::string& phonemes) const {
    std::vector<int64_t> ids;
    const auto cps = utf8_decode(phonemes);
    ids.reserve(cps.size());
    for (uint32_t cp : cps) {
        const auto it = vocab_.find(cp);
        if (it != vocab_.end()) ids.push_back(it->second);
        // fora do vocab: ignora (mesmo comportamento do tokenizer oficial)
        if (ids.size() >= 510) break; // limite do modelo (posições do style)
    }
    return ids;
}

// =============================================================================
// Inferência ONNX
// =============================================================================

std::vector<float> KokoroTTS::run_model_locked(const std::vector<int64_t>& tokens) {
    // input_ids = [0, tokens..., 0]; style = voice[nº de fonemas]; speed.
    std::vector<int64_t> ids;
    ids.reserve(tokens.size() + 2);
    ids.push_back(0);
    ids.insert(ids.end(), tokens.begin(), tokens.end());
    ids.push_back(0);

    const std::size_t style_row = std::min<std::size_t>(tokens.size(), 509);
    const float* style = voice_.data() + style_row * 256;

    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const int64_t ids_shape[2]   = {1, static_cast<int64_t>(ids.size())};
    const int64_t style_shape[2] = {1, 256};
    const int64_t speed_shape[1] = {1};
    float speed = cfg_.speed;

    std::vector<Ort::Value> inputs;
    std::vector<const char*> input_names;
    inputs.emplace_back(Ort::Value::CreateTensor<int64_t>(mem, ids.data(), ids.size(), ids_shape, 2));
    input_names.push_back(input_ids_name_.c_str());
    inputs.emplace_back(Ort::Value::CreateTensor<float>(mem, const_cast<float*>(style), 256, style_shape, 2));
    input_names.push_back(style_name_.c_str());
    inputs.emplace_back(Ort::Value::CreateTensor<float>(mem, &speed, 1, speed_shape, 1));
    input_names.push_back(speed_name_.c_str());

    const char* output_names[] = {output_name_.c_str()};

    try {
        auto outputs = session_->Run(Ort::RunOptions{nullptr}, input_names.data(),
                                     inputs.data(), inputs.size(), output_names, 1);
        const float* data = outputs[0].GetTensorData<float>();
        const std::size_t count = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        return std::vector<float>(data, data + count);
    } catch (const Ort::Exception& e) {
        std::cerr << "[kokoro] inferência falhou: " << e.what() << std::endl;
        return {};
    }
}

std::vector<float> KokoroTTS::synthesize(const std::string& text) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!ensure_loaded_locked()) return {};

    const std::string phonemes = phonemize_locked(clean_for_speech(text));
    if (phonemes.empty()) return {};
    const std::vector<int64_t> tokens = tokenize_locked(phonemes);
    if (tokens.empty()) return {};
    return run_model_locked(tokens);
}

// =============================================================================
// Playback (PortAudio, blocking — mesmo padrão do ElevenLabsTTS)
// =============================================================================

void KokoroTTS::play_audio(const std::vector<float>& samples) {
    if (samples.empty() || !pa_initialized_) return;

    PaStream* stream = nullptr;
    PaError err = Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, SAMPLE_RATE, 256, nullptr, nullptr);
    if (err != paNoError) {
        std::cerr << "[kokoro] Pa_OpenDefaultStream: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    err = Pa_StartStream(stream);
    if (err == paNoError) {
        err = Pa_WriteStream(stream, samples.data(), static_cast<unsigned long>(samples.size()));
        if (err != paNoError && err != paOutputUnderflowed)
            std::cerr << "[kokoro] Pa_WriteStream: " << Pa_GetErrorText(err) << std::endl;
        Pa_StopStream(stream); // blocking: espera o buffer terminar de tocar
    }
    Pa_CloseStream(stream);
}

void KokoroTTS::synthesizeAndPlay(const std::string& text) {
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();

    std::vector<float> samples;
    long long g2p_ms = 0, onnx_ms = 0;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!ensure_loaded_locked()) {
            std::cerr << "[kokoro] TTS não carregado; pulando fala" << std::endl;
            return;
        }
        const std::string cleaned = clean_for_speech(text);
        if (cleaned.empty()) return;

        const std::string phonemes = phonemize_locked(cleaned);
        const std::vector<int64_t> tokens = tokenize_locked(phonemes);
        g2p_ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
        if (tokens.empty()) return;

        samples = run_model_locked(tokens);
        onnx_ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count() - g2p_ms;
    }
    if (samples.empty()) return;

    if (cfg_.verbose_timing) {
        const double audio_s = static_cast<double>(samples.size()) / SAMPLE_RATE;
        const double rtf = audio_s > 0 ? (g2p_ms + onnx_ms) / 1000.0 / audio_s : 0.0;
        std::cout << "\n[kokoro] g2p=" << g2p_ms << "ms onnx=" << onnx_ms
                  << "ms | primeiro áudio em " << (g2p_ms + onnx_ms)
                  << "ms | " << audio_s << "s de fala (RTF=" << rtf << ")" << std::endl;
    }
    play_audio(samples);
}
