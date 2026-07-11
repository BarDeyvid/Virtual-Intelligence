// =============================================================================
// vram_baseline.cpp — Fase 0 do plano VRAMResourceManager
//
// Mede, com números reais (nada de chute):
//   1. Footprint em VRAM de cada modelo (Whisper large-v3, Gemma-3 1B, 4B)
//   2. Tempo de load "frio" (primeira carga do processo, com overhead de
//      driver/contexto CUDA) vs "quente" (RAM já com o arquivo em page cache,
//      contexto CUDA já inicializado)
//   3. Overhead do contexto llama (KV cache) separado do peso do modelo
//   4. Pico combinado (Whisper + LLM residentes ao mesmo tempo)
//
// A VRAM é lida via nvidia-smi (uso global da GPU): rode com o desktop
// parado para minimizar ruído de outros processos.
//
// Saída: tabela em stdout + markdown em plano_scheduler/BASELINE.md
// (caminho configurável pelo argv[1]).
// =============================================================================

#include "llama.h"
#include "whisper.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace {

struct Measurement {
    std::string label;
    long long vram_delta_mib = 0;
    long long load_ms = -1;   // -1 = não se aplica
    long long unload_ms = -1;
    std::string note;
};

std::vector<Measurement> g_results;

// Uso de VRAM global da GPU 0, em MiB. -1 em falha (sem nvidia-smi).
long long read_gpu_used_mib() {
    FILE* pipe = POPEN("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits", "r");
    if (!pipe) return -1;
    char buf[128] = {0};
    std::string out;
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    PCLOSE(pipe);
    try {
        return std::stoll(out);
    } catch (...) {
        return -1;
    }
}

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void report(const std::string& label, long long vram_delta, long long load_ms,
            long long unload_ms, const std::string& note = "") {
    g_results.push_back({label, vram_delta, load_ms, unload_ms, note});
    std::cout << "[baseline] " << label << ": VRAM " << vram_delta << " MiB"
              << ", load " << load_ms << " ms"
              << ", unload " << unload_ms << " ms"
              << (note.empty() ? "" : " (" + note + ")") << std::endl;
}

// --- Whisper -----------------------------------------------------------------

// Carrega e descarrega o Whisper uma vez, medindo tudo.
bool measure_whisper(const std::string& path, const std::string& label,
                     const std::string& note) {
    const long long before = read_gpu_used_mib();
    const long long t0 = now_ms();

    whisper_context_params cparams = whisper_context_default_params();
    whisper_context* ctx = whisper_init_from_file_with_params(path.c_str(), cparams);
    const long long t1 = now_ms();
    if (!ctx) {
        std::cerr << "[baseline] ERRO: falha ao carregar " << path << std::endl;
        return false;
    }
    const long long after = read_gpu_used_mib();

    const long long t2 = now_ms();
    whisper_free(ctx);
    const long long t3 = now_ms();

    report(label, after - before, t1 - t0, t3 - t2, note);
    return true;
}

// --- LLM (llama.cpp) ---------------------------------------------------------

struct LlamaLoadResult {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
};

// Carrega modelo + contexto separadamente para atribuir a VRAM certa a cada um.
LlamaLoadResult measure_llama(const std::string& path, const std::string& label,
                              int n_ctx, bool keep_loaded) {
    const long long before = read_gpu_used_mib();
    const long long t0 = now_ms();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = -1;
    llama_model* model = llama_model_load_from_file(path.c_str(), mparams);
    const long long t1 = now_ms();
    if (!model) {
        std::cerr << "[baseline] ERRO: falha ao carregar " << path << std::endl;
        return {};
    }
    const long long after_model = read_gpu_used_mib();

    std::ostringstream note;
    note << "llama_model_size=" << (llama_model_size(model) / (1024 * 1024)) << " MiB";
    report(label + " (pesos)", after_model - before, t1 - t0, -1, note.str());

    // Contexto/KV cache — o "custo invisível" que não aparece no tamanho do .gguf
    const long long tc0 = now_ms();
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = n_ctx;
    cparams.n_batch = n_ctx;
    llama_context* lctx = llama_init_from_model(model, cparams);
    const long long tc1 = now_ms();
    const long long after_ctx = read_gpu_used_mib();

    if (lctx) {
        report(label + " (contexto n_ctx=" + std::to_string(n_ctx) + ")",
               after_ctx - after_model, tc1 - tc0, -1, "KV cache + buffers de compute");
    } else {
        std::cerr << "[baseline] AVISO: contexto falhou para " << label << std::endl;
    }

    if (keep_loaded) return {model, lctx};

    const long long tu0 = now_ms();
    if (lctx) llama_free(lctx);
    llama_model_free(model);
    const long long tu1 = now_ms();
    report(label + " (unload modelo+ctx)", 0, -1, tu1 - tu0);
    return {};
}

void write_markdown(const std::string& out_path, long long idle_mib,
                    long long total_mib) {
    std::ofstream md(out_path);
    if (!md.is_open()) {
        std::cerr << "[baseline] AVISO: não consegui escrever " << out_path << std::endl;
        return;
    }
    md << "# Fase 0 — Baseline de VRAM (medição real)\n\n";
    md << "Gerado por `vram_baseline` — GPU total: " << total_mib
       << " MiB, uso da GPU antes de qualquer load: " << idle_mib << " MiB.\n\n";
    md << "Leitura via `nvidia-smi` (uso global da GPU): os deltas incluem o\n"
          "overhead de contexto CUDA na primeira medição de cada biblioteca.\n\n";
    md << "| O quê | Δ VRAM (MiB) | Load (ms) | Unload (ms) | Nota |\n";
    md << "|---|---|---|---|---|\n";
    for (const auto& r : g_results) {
        md << "| " << r.label << " | " << r.vram_delta_mib << " | "
           << (r.load_ms < 0 ? std::string("—") : std::to_string(r.load_ms)) << " | "
           << (r.unload_ms < 0 ? std::string("—") : std::to_string(r.unload_ms)) << " | "
           << r.note << " |\n";
    }
    md << "\n## Como ler isso para os tiers\n\n";
    md << "- **Load frio vs quente do Whisper**: a diferença é overhead de init "
          "de contexto CUDA + page cache. O número *quente* é o custo real de "
          "um ciclo JIT (TIER2) em regime.\n";
    md << "- **LLM (pesos) vs (contexto)**: eviction do LLM (Fase 5) paga o "
          "reload dos dois. Se o total for alto, confirma o LLM como TIER1_HOT.\n";
    md << "- **Pico combinado**: se Whisper + LLM + margem couberem juntos com "
          "folga, o JIT do Whisper é opcional (dá pra promover a HOT e zerar a "
          "latência de load por turno).\n";
    std::cout << "[baseline] Resultados salvos em " << out_path << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    const std::string out_path = argc > 1 ? argv[1] : "vram_baseline_results.md";

    // Silencia o log verboso das libs (queremos só a tabela)
    llama_log_set([](ggml_log_level, const char*, void*) {}, nullptr);
    whisper_log_set([](ggml_log_level, const char*, void*) {}, nullptr);

    ggml_backend_load_all();
    llama_backend_init();

    long long total_mib = -1;
    {
        FILE* pipe = POPEN("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits", "r");
        if (pipe) {
            char buf[128] = {0};
            std::string out;
            while (fgets(buf, sizeof(buf), pipe)) out += buf;
            PCLOSE(pipe);
            try { total_mib = std::stoll(out); } catch (...) {}
        }
    }

    const long long idle = read_gpu_used_mib();
    if (idle < 0) {
        std::cerr << "[baseline] ERRO: nvidia-smi indisponível — sem como medir VRAM." << std::endl;
        return 1;
    }
    std::cout << "[baseline] GPU: " << total_mib << " MiB total, " << idle
              << " MiB em uso antes dos loads." << std::endl;

    // --- 1. Whisper: frio, depois quente --------------------------------------
    measure_whisper("models/ggml-large-v3.bin", "Whisper large-v3 (frio)",
                    "1º load do processo: inclui init CUDA");
    measure_whisper("models/ggml-large-v3.bin", "Whisper large-v3 (quente)",
                    "custo real de um ciclo JIT em regime");

    // --- 2. Gemma-3 1B (modelo base / experts) --------------------------------
    measure_llama("models/gemma-3-1b-it-q4_0.gguf", "Gemma-3 1B", 8192, false);

    // --- 3. Gemma-3 4B (Alyssa) ------------------------------------------------
    measure_llama("models/gemma-3-4b-it-q4_0.gguf", "Gemma-3 4B", 8192, false);

    // --- 4. Pico combinado: LLM residente + Whisper JIT ------------------------
    // Cenário real do scheduler: Gemma-4B TIER1_HOT residente enquanto o
    // Whisper carrega para um turno de LISTENING.
    {
        LlamaLoadResult llm = measure_llama("models/gemma-3-4b-it-q4_0.gguf",
                                            "Gemma-3 4B [combinado]", 8192, true);
        if (llm.model) {
            measure_whisper("models/ggml-large-v3.bin",
                            "Whisper large-v3 com LLM residente",
                            "latência JIT sob VRAM já ocupada");
            const long long peak = read_gpu_used_mib();
            report("Pico combinado (LLM residente, pós-JIT)", peak - idle, -1, -1,
                   "uso total acima do idle");
            if (llm.ctx) llama_free(llm.ctx);
            llama_model_free(llm.model);
        }
    }

    llama_backend_free();
    write_markdown(out_path, idle, total_mib);
    return 0;
}
