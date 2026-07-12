// AlyssaCoreAudio.cpp — extensão de áudio nativo do AlyssaCore via mtmd.
//
// Voz direto no gameplayModel (docs/plano-router-e-voz-gameplay.md, B2):
// o enunciado do microfone entra como <__media__> no prompt do tick e o
// Gemma 4 E2B faz ASR+decisão numa inferência só. Corpos fora do header
// de propósito — só quem linka mtmd.lib paga por isto (o resto do
// AlyssaCore segue header-only).

#include "AlyssaCore.hpp"

#include "mtmd.h"
#include "mtmd-helper.h"

#include <chrono>
#include <iostream>

namespace alyssa_core {

void AlyssaCore::free_audio() {
    if (mtmd_ctx) {
        mtmd_free(mtmd_ctx);
        mtmd_ctx = nullptr;
    }
}

bool AlyssaCore::init_audio(const std::string& mmproj_path) {
    if (mtmd_ctx) return true; // idempotente
    if (!model) return false;

    mtmd_context_params mparams = mtmd_context_params_default();
    mparams.use_gpu       = true;
    mparams.print_timings = false;
    mparams.n_threads     = 4;
    mparams.warmup        = false; // primeiro enunciado paga o warmup; boot é mais previsível

    mtmd_ctx = mtmd_init_from_file(mmproj_path.c_str(), model, mparams);
    if (!mtmd_ctx) {
        std::cerr << "[AlyssaCore] mmproj não carregou de '" << mmproj_path
                  << "' — gameplay por voz indisponível." << std::endl;
        return false;
    }
    if (!mtmd_support_audio(mtmd_ctx)) {
        std::cerr << "[AlyssaCore] mmproj carregou mas SEM encoder de áudio "
                     "(mmproj só-visão? use o da ggml-org)." << std::endl;
        free_audio();
        return false;
    }
    std::cout << "[AlyssaCore] Encoder de áudio pronto (mmproj: " << mmproj_path
              << ", taxa " << mtmd_get_audio_sample_rate(mtmd_ctx) << "Hz)" << std::endl;
    return true;
}

std::string AlyssaCore::generate_with_audio(
    const std::string& prompt_with_marker,
    const std::vector<float>& audio_16k,
    const SimpleModelParameters& params,
    std::function<void(const std::string& piece)> stream_callback)
{
    if (!mtmd_ctx) return "Erro: encoder de áudio não inicializado.";
    if (audio_16k.empty()) return "Erro: áudio vazio.";
    last_timed_out = false;
    const auto gen_start = std::chrono::steady_clock::now();

    // O VoicePipeline entrega 16kHz; se o encoder pedir outra taxa, avisa em
    // vez de degradar silenciosamente (Gemma 4 usa 16k — não deve acontecer).
    const int want_rate = mtmd_get_audio_sample_rate(mtmd_ctx);
    if (want_rate > 0 && want_rate != 16000) {
        std::cerr << "[AlyssaCore] AVISO: encoder espera " << want_rate
                  << "Hz e o áudio é 16kHz — qualidade vai sofrer." << std::endl;
    }

    // 1. Sampler chain — mesma receita do generate_raw (grammar primeiro,
    //    min_p, temp, penalties, dist). Duplicado conscientemente: refatorar
    //    o caminho quente do chat não é o objetivo deste patch.
    ScopedSampler smpl(llama_sampler_chain_init(llama_sampler_chain_default_params()));
    float temp = (params.temperature > 0.0) ? static_cast<float>(params.temperature) : 0.8f;
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
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_penalties(64, 1.3f, 0.0f, 0.0f));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // 2. Texto+marcador → chunks (o marcador vira o chunk de áudio)
    mtmd::bitmap audio_bmp(mtmd_bitmap_init_from_audio(audio_16k.size(), audio_16k.data()));
    if (!audio_bmp.ptr) return "Erro: falha ao preparar o áudio.";

    mtmd_input_text text;
    text.text          = prompt_with_marker.c_str();
    text.add_special   = true;  // BOS pelo tokenizer (prompt não traz)
    text.parse_special = true;  // <|turn> e <__media__> são tokens especiais

    mtmd::input_chunks chunks(mtmd_input_chunks_init());
    const mtmd_bitmap* bitmaps[] = { audio_bmp.ptr.get() };
    int32_t rc = mtmd_tokenize(mtmd_ctx, chunks.ptr.get(), &text, bitmaps, 1);
    if (rc != 0) {
        return "Erro: mtmd_tokenize falhou (rc=" + std::to_string(rc) +
               (rc == 1 ? ", marcador <__media__> ausente do prompt?)" : ")");
    }

    // 3. Prefill: texto via llama_decode, áudio via encoder+embeddings
    llama_pos n_past = 0;
    rc = mtmd_helper_eval_chunks(mtmd_ctx, ctx, chunks.ptr.get(),
                                 /*n_past=*/0, /*seq_id=*/0, n_batch,
                                 /*logits_last=*/true, &n_past);
    if (rc != 0) {
        return "Erro: falha ao avaliar prompt com áudio (rc=" + std::to_string(rc) + ").";
    }

    // 4. Loop de geração — cópia do padrão do generate_raw
    std::string response;
    int tokens_generated = 0;
    while (tokens_generated < params.max_tokens) {
        if (params.timeout_ms > 0) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - gen_start).count();
            if (elapsed_ms > params.timeout_ms) {
                last_timed_out = true;
                std::cerr << "[AlyssaCore] Timeout de geração com áudio ("
                          << params.timeout_ms << "ms). Retornando parcial." << std::endl;
                break;
            }
        }

        llama_token new_token_id = llama_sampler_sample(smpl.get(), ctx, -1);
        if (llama_vocab_is_eog(vocab, new_token_id)) break;

        char buf[512];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n < 0) return response; // conversão falhou: devolve o parcial

        std::string piece(buf, n);
        response += piece;
        if (stream_callback) stream_callback(piece);

        llama_token next_batch_tokens[1] = { new_token_id };
        llama_batch batch = llama_batch_get_one(next_batch_tokens, 1);
        if (llama_decode(ctx, batch) != 0) {
            std::cerr << "[AlyssaCore] Falha ao decodificar token (áudio)." << std::endl;
            break;
        }
        tokens_generated++;
    }

    return response;
}

} // namespace alyssa_core
