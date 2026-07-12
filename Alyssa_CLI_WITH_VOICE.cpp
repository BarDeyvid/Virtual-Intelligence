#include "AlyssaNet.hpp"
#include "llama.h"
#include <iostream>
#include <string>
// #include "voice/ElevenLabsTTS.hpp" // TTS anterior (API) — Fase 3 trocou pelo Kokoro local
#include "voice/KokoroTTS.hpp"
#include "voice/VoicePipeline.hpp"
#include <chrono>
#include <mutex>
#include <atomic>
#include "vram/VRAMResourceManager.hpp"
#include "vram/AlyssaResidents.hpp"
#include "vram/GpuMemoryProbe.hpp"
#include "ProactivityEngine.hpp"
#include "TerminalSafety.hpp"
#include "minecraft/MinecraftSession.hpp"

void log_callback(ggml_log_level level, const char * text, void * user_data) {
    (void)level;
    (void)user_data;
    fputs(text, stderr); // Imprime a mensagem de log no stderr
    fflush(stderr);
}

int main(int argc, char** argv) {
    alyssa_safety::install_crash_guard();

    // --minecraft (plano B4): o microfone vira COMANDO DE GAMEPLAY — VAD corta
    // o enunciado e o áudio vai direto pro E2B da Gaia via <__media__> (sem
    // Whisper, sem chat de voz; o chat in-game continua respondido). Rode o
    // sidecar antes: `node index.js` em minecraft-bridge/.
    bool minecraft_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--minecraft") minecraft_mode = true;
    }

    try {
        llama_log_set(log_callback, nullptr);
        ggml_backend_load_all();

        CoreIntegration alyssa_brain;
        std::cout << "Inicializando CoreIntegration..." << std::endl;

        // 1. Chama a Inicialização com o caminho do modelo BASE
        if (!alyssa_brain.initialize("models/gemma-3-1b-it-q4_0.gguf")) {
            std::cerr << "Falha Crítica ao inicializar o CoreIntegration. Encerrando." << std::endl;
            return 1;
        }
        std::cout << "CoreIntegration Inicializado...";

        // Proactivity Engine (portado da CLI de texto): reage à presença via
        // webcam (greeting quando reconhece o usuário) e aos gatilhos de
        // humor/tédio. brain_mtx serializa turnos: think_with_fusion() não é
        // thread-safe, e agora duas fontes podem chamá-lo (loop principal e
        // a thread de proatividade abaixo).
        alyssa_proactivity::ProactivityEngine proactivity(
            alyssa_proactivity::load_proactivity_config());
        std::mutex brain_mtx;
        std::atomic<bool> is_processing{false};

        // Fase 3: Kokoro-82M local (ONNX/CPU) no lugar do ElevenLabs (API).
        // O construtor é barato — o load real (sessão ONNX) é decidido pelo
        // scheduler via TTSResident abaixo (prefetch em THINKING).
        // Voz: blend escolhido de ouvido na audição de 2026-07-03 (candidata
        // 3) — pf_dora ancora o pt-br, af_bella deixa o timbre mais jovem.
        // Alternativas: test_kokoro --audition / --voice "spec" --play.
        KokoroTTS::Config tts_cfg;
        tts_cfg.voice_blend = {{"models/kokoro/voices/pf_dora.bin", 0.5f},
                               {"models/kokoro/voices/af_bella.bin", 0.5f}};
        KokoroTTS tts(tts_cfg);

        // Whisper com load adiado: quem decide quando ele entra/sai da VRAM é
        // o scheduler (plano_scheduler/). TIER2_JIT: entra em LISTENING
        // (disparado pelo VAD, sobreposto à fala do usuário) e sai em THINKING.
        // STT "nitro": Silero VAD streaming (CPU, fora do orçamento de VRAM)
        // permite janela de silêncio de 450ms em vez de 1s; beam search +
        // initial_prompt pt-BR sulista (defaults de Options) seguram o sotaque.
        // Endpoint semântico plugável via stt.set_endpoint_probe() (futuro:
        // smart-turn ONNX). Sem models/ggml-silero-v5.1.2.bin o pipeline cai
        // no RMS antigo e realarga a janela sozinho.
        VoicePipeline::Options stt_opts;
        stt_opts.n_threads      = 4;
        stt_opts.vad_silence_ms = 450;
        stt_opts.vad_only       = minecraft_mode; // gameplay: só VAD, zero Whisper
        // large-v3-turbo q8_0 (medido 2026-07-12, docs/benchmarks-2026-07-11.md):
        // load 925ms vs 2803ms do large-v3 cheio (o load JIT some dentro da
        // fala), transcrição ~2x mais rápida, 874MB vs 3GB de VRAM, saída
        // idêntica no áudio de teste pt-BR. O .bin antigo continua em models/
        // como rollback.
        VoicePipeline stt("models/ggml-large-v3-turbo-q8_0.bin", stt_opts,
                          /*defer_model_load=*/true);

        // =====================================================================
        // VRAMResourceManager (plano_scheduler/).
        //
        // INCIDENTE 2026-07-04: com Whisper TIER1_HOT + desktop real (~6.3GiB:
        // jogo/browser/LM Studio, não os 2.2GiB "idle" da Fase 0), o uso chegou
        // a 15.6/16GB → paging WDDM → driver preso em DPC → BSOD
        // DPC_WATCHDOG_VIOLATION. Regra revista (BASELINE.md): TIER1_HOT só
        // para o LLM; Whisper volta a JIT (paga ~2.7s/turno, parcialmente
        // escondidos pelo load durante a fala). Kokoro TTS segue HOT porque
        // custa ZERO de VRAM (ONNX Runtime CPU-only).
        // =====================================================================
        using namespace alyssa::vram;
        constexpr std::size_t GiB = 1024ull * 1024 * 1024;
        constexpr std::size_t MiB = 1024ull * 1024;

        // Fase 5: o manager enxerga a VRAM livre REAL (NVML) e tem um
        // watchdog que reage se apps externos comerem o orçamento no meio
        // da sessão (a causa do BSOD). Defaults da política: piso de 1.5GiB
        // livres, poll de 1s, eviction de HOT permitida com reload garantido.
        VRAMResourceManager vram_manager(16 * GiB, EvictionPolicy{});

        auto llm_resident = std::make_shared<LLMResident>(
            [&alyssa_brain] { return alyssa_brain.llm_vram_footprint_bytes(); });
        vram_manager.register_resident(llm_resident);
        // turbo-q8: ~874MB de pesos + ~200MB de estado/compute. Segue JIT por
        // ora (regra pós-incidente 2026-07-04: TIER1_HOT só p/ o LLM), mas com
        // ~1.1GiB virou candidato a voltar a HOT se a pressão real permitir.
        auto whisper_resident = std::make_shared<WhisperResident>(
            &stt, 1100 * MiB, ResidencyTier::TIER2_JIT);
        vram_manager.register_resident(whisper_resident);
        // Fase 3: lambdas plugadas no TTSResident — o scheduler não muda.
        // footprint_bytes = 0 porque o ORT do vcpkg é CPU-only (a sessão vive
        // em ~330MiB de RAM, não concorre com o orçamento de VRAM). TIER1_HOT
        // pela medição: load quente ~1.5s (> 500ms do critério da Fase 0) e
        // residência de custo zero em VRAM — ver BASELINE.md (Fase 3).
        auto tts_resident = std::make_shared<TTSResident>(
            /*footprint_bytes=*/0,
            /*loader=*/[&tts] { return tts.load(); },
            /*unloader=*/[&tts] { return tts.unload(); },
            ResidencyTier::TIER1_HOT);
        vram_manager.register_resident(tts_resident);

        // Telemetria: transições e loads viram log (futuro: EndocrineSystem
        // reagindo a load lento — "hesitação" in-character; ver Fase 5).
        vram_manager.set_state_transition_callback([](AlyssaState from, AlyssaState to) {
            std::cout << "[vram] " << to_string(from) << " -> " << to_string(to) << std::endl;
        });
        vram_manager.set_resident_status_callback(
            [](std::string_view id, ResidentStatus o, ResidentStatus n) {
                std::cout << "[vram] '" << id << "': " << to_string(o)
                          << " -> " << to_string(n) << std::endl;
            });

        // Dispara um gatilho de proatividade agora, se houver um pendente e o
        // cérebro estiver livre — mesmo padrão da CLI de texto. Usado tanto
        // pelo poll periódico (humor/tédio) quanto pelo reconhecimento
        // instantâneo de presença (on_presence_frame): responsividade em
        // primeiro lugar, o welcome-back não espera o próximo tick.
        auto fire_proactive = [&]() {
            if (is_processing) return;

            auto profile = alyssa_brain.get_endocrine_system()->get_hormone_profile();
            auto trigger = proactivity.check(profile);
            if (trigger.type == alyssa_proactivity::TriggerType::None) return;

            std::unique_lock<std::mutex> lock(brain_mtx, std::try_to_lock);
            if (!lock.owns_lock() || is_processing) return; // cérebro ocupado, tenta na próxima chance
            is_processing = true;

            stt.pause();
            std::string msg = alyssa_brain.generate_proactive_message(trigger.reason);
            proactivity.note_proactive_message();
            if (!msg.empty()) {
                std::cout << " [Alyssa, por conta própria]: " << msg << std::endl;
                tts.synthesizeAndPlay(msg);
            }
            stt.resume();
            is_processing = false;
        };

        // Streaming ao vivo always-on: cada frame publicado pelo stream cai
        // aqui (thread do stream). Alimenta a proatividade (throttled a 1x/s)
        // — mesmo mecanismo da CLI de texto. O welcome-back dispara na hora,
        // direto daqui, em vez de esperar o próximo tick do poll periódico.
        auto on_presence_frame = [&alyssa_brain, &proactivity, &fire_proactive]() {
            auto* det = alyssa_brain.get_presence_detector();
            if (!det) return;
            auto presence = det->latest();
            if (!proactivity.config().presence_detection || !presence.available) return;

            static auto last_feed = std::chrono::steady_clock::time_point{};
            auto now = std::chrono::steady_clock::now();
            if (now - last_feed < std::chrono::seconds(1)) return;
            last_feed = now;

            bool was_present = proactivity.user_present();
            proactivity.note_presence(presence.present);
            if (presence.present && !was_present) {
                // Usuário voltou: oxitocina sobe (rever quem se gosta)
                alyssa_brain.get_endocrine_system()->trigger_social_response(0.3);
            }
            // Chama sempre que presente (não só na transição): user_present()
            // retorna true por padrão antes da 1ª leitura, então o "primeiro
            // avistamento" nunca bateria como transição ausente->presente.
            // check() é barato quando não há nada pendente.
            if (presence.present) fire_proactive();
        };

        // Fase 2: com o throttle ligado, LISTENING/SPEAKING pausam captura
        // contínua de vision (se houver stream de webcam ativo, ele para).
        // Always-on: ao sair do estado de pressão o hook religa sozinho.
        vram_manager.set_vision_pause_hook([&alyssa_brain, on_presence_frame](bool paused) {
            auto* det = alyssa_brain.get_presence_detector();
            if (!det || !det->is_ready()) return;
            if (paused) {
                if (det->is_streaming()) {
                    det->stop_stream();
                    std::cout << "[vram] Vision pausado (webcam liberada durante STT/TTS)" << std::endl;
                }
            } else if (!det->is_streaming()) {
                det->start_stream(on_presence_frame);
                std::cout << "[vram] Vision retomado (webcam sempre ativa)" << std::endl;
            }
        });
        vram_manager.set_vision_throttle_enabled(true);

        // Webcam always-on: liga assim que o detector está pronto (sem
        // esperar por um estado específico) e fica ativa pelo resto da sessão,
        // exceto durante as pausas do throttle acima.
        if (auto* det = alyssa_brain.get_presence_detector(); det && det->is_ready()) {
            det->start_stream(on_presence_frame);
        }

        // Marca o LLM como residente: o modelo já foi carregado pelo
        // initialize(); aqui o resident só confirma e registra o footprint.
        if (!llm_resident->load_async().get()) {
            std::cerr << "AVISO: LLMResident não confirmou residência do modelo." << std::endl;
        }
        std::cout << "[vram] LLM residente (TIER1_HOT), footprint "
                  << (llm_resident->metrics().vram_bytes_footprint >> 20)
                  << " MiB. Orçamento: 16GiB." << std::endl;

        // Kokoro TIER1_HOT: carrega em background agora (não bloqueia o boot;
        // é RAM, não VRAM). O Whisper (JIT) NÃO pré-carrega — ele entra via
        // VAD→LISTENING, sobreposto à fala, e sai da VRAM entre turnos.
        {
            auto tts_load = tts_resident->load_async();
            (void)tts_load; // promise-based: descartar não bloqueia
        }

        // Sanidade de orçamento: a Fase 0 mediu a Alyssa inteira em ~10GiB de
        // VRAM (whisper JIT no pico + LLMs residentes). Se o resto do desktop
        // já ocupa demais, avisa ANTES de estourar (incidente 2026-07-04).
        if (const auto gpu = query_gpu_memory()) {
            const std::size_t external_used = gpu->used_bytes;
            if (external_used > 5 * GiB) {
                std::cout << "\n[vram] AVISO: a GPU já está com " << (external_used >> 20)
                          << " MiB em uso por outros apps. A Alyssa precisa de ~10GiB "
                          << "no pico — acima de ~5GiB externos há risco de paging/"
                          << "travamento do driver. Feche jogo/apps pesados.\n" << std::endl;
            }
        }

        // Fase 5: pressão de VRAM vira reação in-character — cortisol sobe
        // (a "hesitação" prevista no plano) além do log. O callback roda na
        // thread do watchdog; o EndocrineSystem não é thread-safe, mas o
        // ajuste é um double (pior caso: um tick hormonal impreciso — XGH).
        vram_manager.set_pressure_callback(
            [&alyssa_brain](std::size_t free_bytes, std::size_t floor_bytes) {
                std::cout << "[vram] pressão externa de VRAM (livre "
                          << (free_bytes >> 20) << " MiB < piso "
                          << (floor_bytes >> 20) << " MiB) — degradando p/ modo JIT"
                          << std::endl;
                if (auto* endo = alyssa_brain.get_endocrine_system()) {
                    endo->trigger_stress_response(0.35);
                }
            });

        // Início de fala detectado pelo VAD → LISTENING: o load do Whisper
        // corre em paralelo com o usuário ainda falando (pipelining, doc §3).
        stt.set_on_speech_start([&vram_manager] {
            if (vram_manager.current_state() != AlyssaState::LISTENING) {
                auto f = vram_manager.request_state(AlyssaState::LISTENING);
                (void)f; // future promise-based: descartar não bloqueia
            }
        });

        // Minecraft por voz (plano B4): sessão + roteamento dos enunciados.
        std::unique_ptr<alyssa_minecraft::MinecraftSession> minecraft_session;
        if (minecraft_mode) {
            minecraft_session = std::make_unique<alyssa_minecraft::MinecraftSession>(
                alyssa_brain, brain_mtx);
            if (!minecraft_session->start()) {
                std::cerr << "[Minecraft] Sidecar fora do ar (node index.js em "
                             "minecraft-bridge/) — encerrando." << std::endl;
                return 1;
            }
            stt.set_on_segment([&minecraft_session](const std::vector<float>& audio,
                                                    const std::string&, long long) {
                if (minecraft_session && minecraft_session->is_running()) {
                    std::cout << "[Minecraft] Voz capturada ("
                              << audio.size() / 16000.0 << "s) → próximo tick" << std::endl;
                    minecraft_session->set_pending_voice(audio);
                }
            });
            std::cout << "\n=== MODO MINECRAFT: fale comandos (\"ataca o zumbi\", "
                         "\"vai pra árvore\") — a Gaia ouve direto ===\n" << std::endl;
        }

        if (!stt.start()) {
            std::cerr << "Falha ao inciar Pipeline TTS." << std::endl;
            return 1;
        }

        // Thread de proatividade (portada da CLI de texto): dorme em passos
        // de 1s e só age a cada check_interval_s. try_lock em brain_mtx: se
        // o usuário estiver falando/sendo respondido, ela não interrompe.
        std::thread proactivity_thread([&]() {
            const int interval = proactivity.config().check_interval_s;
            int elapsed = 0;
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (++elapsed < interval) continue;
                elapsed = 0;
                fire_proactive(); // gatilhos de humor (tédio/estresse/empolgação); presença já é reativa
            }
        });
        proactivity_thread.detach();

        std::string user_input;

        while (true) {
            // No modo minecraft não há transcrição de chat — o prompt "> " a
            // cada 50ms só afogava o console.
            if (!minecraft_mode) printf("\033[32m> \033[0m");
            if (stt.get_last_result(user_input)) {
                std::lock_guard<std::mutex> lock(brain_mtx);
                is_processing = true;
                std::cout << " [Transcrição]: " << user_input << std::endl;
                proactivity.note_user_activity();
                stt.pause();

                // THINKING: descarrega o Whisper em paralelo com a inferência
                // (doc §3 — unload_async(whisper) sobreposto ao trabalho útil)
                auto thinking = vram_manager.request_state(AlyssaState::THINKING);
                (void)thinking;

                // Fase 4: prefetch do TTS junto com a inferência. Com o Kokoro
                // TIER1_HOT isto é normalmente no-op (já residente); fica como
                // rede de segurança se um load anterior falhou — o load da
                // sessão ONNX (CPU/RAM) se sobrepõe à inferência do LLM.
                vram_manager.prefetch("tts");

                //  Usa Weighted Fusion em vez de think simples
                // Latência ponta-a-ponta (Fase 3/4): STT pronto → resposta
                // falada. O primeiro áudio real é logado pelo próprio Kokoro
                // ([kokoro] primeiro áudio em Xms) na 1ª sentença do stream.
                const auto t_stt_done = std::chrono::steady_clock::now();
                std::string alyssa_response = alyssa_brain.think_with_fusion(user_input, tts);
                const auto turn_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t_stt_done).count();
                std::cout << "[latencia] STT -> fim da fala: " << turn_ms << "ms" << std::endl;

                // Resposta falada → volta a IDLE (só LLM residente) e reabre o mic
                auto idle = vram_manager.request_state(AlyssaState::IDLE);
                (void)idle;
                stt.resume();
                is_processing = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
       }

    } catch (const std::exception& e) {
        std::cerr << "\n[ERRO FATAL] Exceção não capturada: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n[ERRO FATAL] Erro desconhecido não capturado." << std::endl;
        return 1;
    }
    return 0;
}
