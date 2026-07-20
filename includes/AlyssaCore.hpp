/**
 * @file AlyssaCore.hpp
 * @brief Header file for the AlyssaCore class.
 *
 * Contains the definition of the AlyssaCore class, which is responsible for loading and managing a language model,
 * generating text based on prompts, and applying LoRA (Low-Rank Adaptation) if necessary.
 */

#pragma once

// MSVC treats POSIX names like strdup() as deprecated in favour of _strdup().
// We suppress the warning because strdup is universally supported and makes
// cross-platform code cleaner. The real fix (using std::string exclusively)
// is covered under Phase 4 of the refactor.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "llama.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include "json.hpp"
#include <fstream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <functional>
#include <memory>

using json = nlohmann::json;

// Encoder multimodal (áudio nativo do Gemma 4). Forward declaration em escopo
// GLOBAL — o tipo real vive em tools/mtmd/mtmd.h; só AlyssaCoreAudio.cpp o vê.
struct mtmd_context;

/**
 * @struct SimpleModelParameters
 * @brief Structure to hold parameters for the model generation.
 */
struct SimpleModelParameters {
    double temperature = 0.8; /**< Temperature parameter for controlling randomness in text generation. */
    double top_p = 1.0;         /**< Top-p (nucleus) sampling parameter for diverse output. */
    int max_tokens = 512;       /**< Maximum number of tokens to generate. */
    int timeout_ms = 0;         /**< Generation time budget in ms; 0 = unlimited (Phase 4.3). */
    std::string grammar;         /**< Optional GBNF grammar source; empty = unconstrained sampling. */
    std::string grammar_root = "root"; /**< Start rule name within `grammar`. */

    /// Penalidade de repetição (v2/F3): 1.3/64 era HARDCODED no generate_raw.
    /// 1.3 é agressivo — o 1B (logits achatados) vira sopa de token em prosa
    /// PT-BR longa, porque a janela penaliza até as palavras comuns do FIM DO
    /// PROMPT antes do primeiro token gerado. Defaults preservam o
    /// comportamento da persona; sumarização/consolidação passam 1.05.
    double repeat_penalty = 1.3; /**< 1.0 = desligado. */
    int penalty_last_n = 64;     /**< Janela de tokens considerada. */
};

/**
 * @struct SimpleModelConfig
 * @brief holds configuration details for a simple model.
 */
struct SimpleModelConfig {
    std::string id;             /**< Unique identifier for the model. */
    std::string model_path;       /**< Path to the base model file. */
    std::string system_prompt;    /**< System prompt for initializing the model context. */
    std::string role_instruction; /**< Instruction or role assigned to the model. */
    bool usa_LoRA = false;              /**< Flag indicating if LoRA adaptation should be used. */
    std::string lora_path;        /**< Path to the LoRA file (if applicable). */
    SimpleModelParameters params;  /**< Parameters for model generation. */
    int n_ctx = 8192;            /**< Context size for the model. */
    int n_batch = 8192;           /**< Batch size for processing tokens. */
};

using AllModelConfigs = std::vector<SimpleModelConfig>;

// =============================================================================
// RAII wrapper for llama_chat_message history
// =============================================================================

/**
 * @brief Appends a new message to a llama_chat_message history vector.
 *
 * Allocates the content string with strdup and pushes the message.  The
 * caller owns the vector and must call free_chat_history() when done, OR
 * rely on AlyssaCore's clear helpers which call that function.
 *
 * Using this helper (instead of raw strdup + push_back scattered around the
 * codebase) makes ownership explicit and grep-able.
 */
inline void push_chat_message(
    std::vector<llama_chat_message>& history,
    const char* role,
    const std::string& content
) {
    history.push_back({role, strdup(content.c_str())});
}

/**
 * @brief Frees all strdup-allocated content strings and empties the vector.
 */
inline void free_chat_history(std::vector<llama_chat_message>& history) {
    for (auto& msg : history) {
        free(const_cast<char*>(msg.content));
    }
    history.clear();
}

/**
 * @brief Function to load model configurations from a JSON file.
 *
 * Reads and parses a JSON configuration file containing details about multiple models.
 * Each model's configuration is stored in a SimpleModelConfig object and added to a vector.
 *
 * @return A vector of SimpleModelConfig objects representing the loaded configurations.
 */
inline AllModelConfigs load_config() {
    AllModelConfigs configs;
    const std::string filepath = "config/ConfigsLLM.json";
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERRO: Não foi possível abrir o arquivo de configuração: " << filepath << std::endl;
        return configs;
    }

    try {
        json j = json::parse(file);

        if (j.contains("models") && j["models"].is_array()) {
            for (const auto& model_json : j["models"]) {
                SimpleModelConfig config;

                // Campos de nível superior
                if (model_json.contains("id")) {
                    config.id = model_json["id"].get<std::string>();
                }
                if (model_json.contains("model_path")) {
                    config.model_path = model_json["model_path"].get<std::string>();
                }
                if (model_json.contains("system_prompt")) {
                    config.system_prompt = model_json["system_prompt"].get<std::string>();
                }
                if (model_json.contains("role_instruction")) {
                    config.role_instruction = model_json["role_instruction"].get<std::string>();
                }
                if (model_json.contains("usa_LoRA")) {
                    config.usa_LoRA = model_json["usa_LoRA"].get<bool>();
                }
                if (model_json.contains("lora_path")) {
                    config.lora_path = model_json["lora_path"].get<std::string>();
                }
                if (model_json.contains("n_ctx")) {
                    config.n_ctx = model_json["n_ctx"].get<int>();
                }
                if (model_json.contains("n_batch")) {
                    config.n_batch = model_json["n_batch"].get<int>();
                }

                // Extrair campos aninhados "parametros"
                if (model_json.contains("parametros") && model_json["parametros"].is_object()) {
                    const auto& params_json = model_json["parametros"];
                    
                    if (params_json.contains("temperatura")) {
                        config.params.temperature = params_json["temperatura"].get<double>();
                    }
                    if (params_json.contains("top_p")) {
                        config.params.top_p = params_json["top_p"].get<double>();
                    }
                    if (params_json.contains("max_tokens")) {
                        config.params.max_tokens = params_json["max_tokens"].get<int>();
                    }
                    if (params_json.contains("timeout_ms")) {
                        config.params.timeout_ms = params_json["timeout_ms"].get<int>();
                    }
                    if (params_json.contains("grammar_file")) {
                        std::ifstream gfile(params_json["grammar_file"].get<std::string>());
                        if (gfile.is_open()) {
                            config.params.grammar.assign(
                                std::istreambuf_iterator<char>(gfile), std::istreambuf_iterator<char>());
                        } else {
                            std::cerr << "ERRO: grammar_file não encontrado para " << config.id << std::endl;
                        }
                    }
                    if (params_json.contains("grammar_root")) {
                        config.params.grammar_root = params_json["grammar_root"].get<std::string>();
                    }
                }
                configs.push_back(config);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ERRO ao carregar config: " << e.what() << std::endl;
    }

    return configs;
}

namespace alyssa_core {

    /**
     * @brief RAII Wrapper for llama_sampler to prevent memory leaks during exceptions.
     */
    struct SamplerDeleter {
        void operator()(llama_sampler* s) const { if (s) llama_sampler_free(s); }
    };
    using ScopedSampler = std::unique_ptr<llama_sampler, SamplerDeleter>;

    /**
     * @class AlyssaCore
     * @brief Class to manage language models and generate text.
     */
    class AlyssaCore {
    private:
        llama_model* model;              /**< Pointer to the loaded base model. */
        const llama_vocab* vocab;         /**< Pointer to the vocabulary used by the model. */
        llama_context* ctx;              /**< Pointer to the unique shared context. */
        int n_ctx;                        /**< Size of the context. */
        int n_batch;                      /**< Size of the batch. */
        bool owns_model = true;           /**< false = context-only wrapper over a shared model. */
        bool last_timed_out = false;      /**< Set when generate_raw() hits its time budget. */
        ::mtmd_context* mtmd_ctx = nullptr; /**< Encoder de áudio (mmproj); nullptr = sem áudio. */

        /// Libera o mtmd_ctx (definido em AlyssaCoreAudio.cpp; no-op se nulo).
        void free_audio();

    public:
        /**
         * @brief Constructor for AlyssaCore.
         */
        AlyssaCore(const std::string& base_model_path, int context_size = 2048, int batch_size = 8192) 
            : model(nullptr), vocab(nullptr), ctx(nullptr), n_ctx(context_size), n_batch(batch_size) 
        {
            llama_model_params mParams = llama_model_default_params();
            mParams.n_gpu_layers = -1; 

            model = llama_model_load_from_file(base_model_path.c_str(), mParams);
            if (!model) {
                throw std::runtime_error("Falha ao carregar o modelo BASE: " + base_model_path);
            }
            std::cout << "Modelo BASE carregado com sucesso: " << base_model_path << std::endl;
            
            vocab = llama_model_get_vocab(model);

            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = n_ctx;
            ctx_params.n_batch = n_batch;
            // v2/F3: cache SWA COMPLETO. Com o cache otimizado (default), o
            // gemma3 1B (n_swa=1024) vira sopa de token em prompts maiores
            // que a janela — reproduzido em tests/test_utility_gen.cpp (D).
            // Custo de VRAM irrisório nos nossos modelos; protege também
            // históricos longos da persona.
            ctx_params.swa_full = true;

            ctx = llama_init_from_model(model, ctx_params);
            if (!ctx) {
                throw std::runtime_error("AlyssaCore: Falha ao criar contexto.");
            }
            std::cout << "Contexto criado com n_ctx = " << n_ctx << " e n_batch = " << n_batch << std::endl;
        }

        /**
         * @brief Context-only constructor over an already loaded model (Phase 3.2).
         *
         * Creates an additional llama_context on a model owned elsewhere.
         * llama.cpp supports multiple contexts per model, each usable from its
         * own thread — this is what makes parallel expert execution possible
         * without duplicating weights in RAM/VRAM.
         *
         * IMPORTANT: the wrapper must be destroyed BEFORE the owning AlyssaCore.
         */
        AlyssaCore(llama_model* shared_model, int context_size, int batch_size)
            : model(shared_model), vocab(nullptr), ctx(nullptr),
              n_ctx(context_size), n_batch(batch_size), owns_model(false)
        {
            if (!model) {
                throw std::runtime_error("AlyssaCore: modelo compartilhado nulo.");
            }
            vocab = llama_model_get_vocab(model);

            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = n_ctx;
            ctx_params.n_batch = n_batch;
            ctx_params.swa_full = true; // mesmo fix anti-sopa do construtor principal

            ctx = llama_init_from_model(model, ctx_params);
            if (!ctx) {
                throw std::runtime_error("AlyssaCore: Falha ao criar contexto compartilhado.");
            }
            std::cout << "Contexto adicional criado (modelo compartilhado, n_ctx = "
                      << n_ctx << ")" << std::endl;
        }

        /**
         * @brief Destructor for AlyssaCore.
         */
        ~AlyssaCore() {
            free_audio(); // antes do ctx/model: o mtmd referencia o modelo
            if (ctx) llama_free(ctx);
            if (model && owns_model) llama_model_free(model);
            if (owns_model) std::cout << "Modelo BASE e Contexto liberados." << std::endl;
        }

        /**
         * @brief Clear this context's KV cache (all sequences).
         * @details Pool contexts are reused across turns/experts; without this
         *          the previous expert's tokens would contaminate the next run.
         */
        void clear_kv() {
            if (!ctx) return;
            llama_memory_seq_rm(llama_get_memory(ctx), -1, -1, -1);
        }

        llama_model* get_model() { return model; }
        const llama_vocab* get_vocab() { return vocab; }
        llama_context* get_context() { return ctx; }
        int get_n_ctx() { return n_ctx; }
        int get_n_batch() { return n_batch; }

        // =====================================================================
        // Áudio nativo via mtmd (Gemma 4 E2B — voz direto no gameplay).
        // Implementação em includes/AlyssaCoreAudio.cpp: o resto da classe é
        // header-only, mas os corpos que tocam mtmd ficam fora do header pra
        // só quem linka mtmd.lib pagar por eles (docs/plano-router-e-voz-
        // gameplay.md, B2).
        // =====================================================================

        /**
         * @brief Carrega o encoder de áudio (mmproj) pra este core. Lazy e
         *        idempotente; ~557MB de VRAM (Q8). false = áudio indisponível.
         */
        bool init_audio(const std::string& mmproj_path);

        /// true quando init_audio() já carregou o encoder.
        bool audio_ready() const { return mtmd_ctx != nullptr; }

        /**
         * @brief Como generate_raw(), mas o prompt contém UM marcador
         *        <__media__> que vira o áudio (16kHz mono float).
         * @details O prompt deve vir já no turn format do Gemma 4
         *          (ExpertBase::format_gemma4_prompt) — NUNCA o template do
         *          GGUF (minja 0xC0000409) nem o do Gemma 3 (saída vazia em
         *          áudio real; ver docs/gemma4-migration.md).
         */
        std::string generate_with_audio(
            const std::string& prompt_with_marker,
            const std::vector<float>& audio_16k,
            const SimpleModelParameters& params,
            std::function<void(const std::string& piece)> stream_callback = nullptr);

        /// true when the last generate_raw() hit its time budget (Phase 4.3).
        bool last_generation_timed_out() const { return last_timed_out; }

        /**
         * @brief Generates text based on a given prompt (the new part of the conversation).
         *
         * @param prompt The input prompt fragment to process.
         * @param params Parameters for controlling the text generation.
         * @param lora Pointer to LoRA adapter (can be nullptr).
         * @param stream_callback Callback function to stream generated tokens (optional).
         * @return Generated text as a string.
         */
        std::string generate_raw(
            const std::string & prompt,
            const SimpleModelParameters& params,
            llama_adapter_lora** lora, 
            std::function<void(const std::string& piece)> stream_callback
        ) {
            std::string response;
            llama_batch batch;
            last_timed_out = false;
            const auto gen_start = std::chrono::steady_clock::now();

            // 1. RAII Sampler to prevent leaks
            ScopedSampler smpl(llama_sampler_chain_init(llama_sampler_chain_default_params()));
            
            float top_p = (params.top_p > 0.0) ? static_cast<float>(params.top_p) : 1.0f;
            float temp = (params.temperature > 0.0) ? static_cast<float>(params.temperature) : 0.8f;

            // Grammar goes first: it masks invalid-token logits to -inf so every
            // downstream sampler (min_p/temp/penalties/dist) only ever sees the
            // subset of tokens the GBNF grammar still allows at this position.
            if (!params.grammar.empty()) {
                llama_sampler* grammar = llama_sampler_init_grammar(
                    vocab, params.grammar.c_str(), params.grammar_root.c_str());
                if (grammar) {
                    llama_sampler_chain_add(smpl.get(), grammar);
                } else {
                    std::cerr << "[AlyssaCore] Falha ao inicializar grammar (root=\""
                              << params.grammar_root << "\"); gerando sem restrição." << std::endl;
                }
            }

            llama_sampler_chain_add(smpl.get(), llama_sampler_init_min_p(0.05f, 1));
            llama_sampler_chain_add(smpl.get(), llama_sampler_init_temp(temp));
            llama_sampler_chain_add(smpl.get(), llama_sampler_init_penalties(
                params.penalty_last_n, static_cast<float>(params.repeat_penalty), 0.0f, 0.0f));
            llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

            // 2. Tokenize the NEW prompt part
            std::vector<llama_token> tokens(prompt.size() + 1);
            int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, true);
            if (n_tokens < 0) throw std::runtime_error("Falha ao tokenizar o prompt.");
            tokens.resize(n_tokens);

            // 3 & 4. Prepare batch and Decode in chunks to prevent GGML_ASSERT(n_tokens <= n_batch)
            for (int i = 0; i < n_tokens; i += n_batch) {
                int chunk_size = std::min(n_batch, n_tokens - i);
                llama_batch batch = llama_batch_get_one(tokens.data() + i, chunk_size);

                if (llama_decode(ctx, batch) != 0) {
                    throw std::runtime_error("Falha ao decodificar prompt chunk.");
                }
            }

            // 5. Generation Loop
            int tokens_generated = 0;
            while (tokens_generated < params.max_tokens) {
                // Orçamento de tempo (Fase 4.3): estourou → devolve o parcial.
                // Melhor resposta truncada em Ns do que UI travada indefinidamente.
                if (params.timeout_ms > 0) {
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - gen_start).count();
                    if (elapsed_ms > params.timeout_ms) {
                        last_timed_out = true;
                        std::cerr << "[AlyssaCore] Timeout de geração (" << params.timeout_ms
                                  << "ms) após " << tokens_generated
                                  << " tokens. Retornando resposta parcial." << std::endl;
                        break;
                    }
                }

                llama_token new_token_id = llama_sampler_sample(smpl.get(), ctx, -1);

                if (llama_vocab_is_eog(vocab, new_token_id)) break;

                // Convert token to string piece
                char buf[512];
                int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                if (n < 0) throw std::runtime_error("Falha ao converter token.");

                std::string piece(buf, n);
                response += piece;
                
                if (stream_callback) {
                    stream_callback(piece);
                }

                // Prepare batch for the single newly generated token for next step
                llama_token next_batch_tokens[1] = { new_token_id };
                batch = llama_batch_get_one(next_batch_tokens, 1);

                if (llama_decode(ctx, batch) != 0) {
                    throw std::runtime_error("Falha ao decodificar token gerado.");
                }

                tokens_generated++;
            }

            return response;
        }

    };
}
