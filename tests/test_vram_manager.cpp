// =============================================================================
// test_vram_manager.cpp — testes do VRAMResourceManager com residents stub.
//
// Cobre as entregas verificáveis sem GPU:
//   Fase 1: IDLE → LISTENING → THINKING → SPEAKING → IDLE; LLM (TIER1_HOT)
//           nunca recarrega entre turnos; idempotência de load/unload.
//   Fase 2: throttle de vision pausa em LISTENING/SPEAKING, retoma em
//           IDLE/THINKING (e nunca fica pausado ao desligar o throttle).
//   Fase 4: prefetch("tts") deixa o resident quente antes do SPEAKING
//           (sem load duplicado) e cancel_prefetch descarrega; prefetch é
//           descartado sob pressão de orçamento.
//   Fase 5: fits considera a VRAM livre REAL (probe fake injetado); eviction
//           em cascata no load formal com reload garantido do TIER1; unload
//           recusado não trava o manager; watchdog de pressão com histerese.
//
// Os testes das fases 1-4 rodam com probe/watchdog DESLIGADOS (política
// "quiet") para serem determinísticos em qualquer máquina; os da Fase 5
// injetam um probe fake controlado pelo teste.
// =============================================================================

#include "vram/VRAMResourceManager.hpp"
#include "vram/ResidentBase.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace alyssa::vram;
using namespace std::chrono_literals;

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (cond) {                                                          \
            std::cout << "  [OK] " << msg << std::endl;                      \
        } else {                                                             \
            std::cout << "  [FALHOU] " << msg << " (linha " << __LINE__      \
                      << ")" << std::endl;                                   \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

// Resident stub: load/unload dormem um pouco (simula PCIe) e contam chamadas.
class StubResident : public ResidentBase {
public:
    StubResident(std::string id, ResidencyTier tier, std::size_t footprint,
                 std::chrono::milliseconds op_time = 30ms)
        : ResidentBase(std::move(id), tier, footprint), op_time_(op_time) {}

    std::atomic<int> load_count{0};
    std::atomic<int> unload_count{0};

protected:
    bool do_load() override {
        std::this_thread::sleep_for(op_time_);
        ++load_count;
        return true;
    }
    bool do_unload() override {
        std::this_thread::sleep_for(op_time_);
        ++unload_count;
        return true;
    }

private:
    std::chrono::milliseconds op_time_;
};

// Espera uma condição virar true (ops assíncronas de unload não têm future
// exposto pelo request_state — poll com timeout resolve).
bool wait_for(std::function<bool()> cond, std::chrono::milliseconds timeout = 2000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return cond();
}

constexpr std::size_t MiB = 1024ull * 1024;
constexpr std::size_t GiB = 1024ull * MiB;

// Política determinística p/ as fases 1-4: sem probe real, sem watchdog.
EvictionPolicy quiet_policy() {
    EvictionPolicy p;
    p.use_gpu_probe = false;
    p.pressure_poll_ms = 0;
    return p;
}

} // namespace

int main() {
    std::cout << "=== test_vram_manager ===" << std::endl;

    // Orçamento de 16GB (a 5060 Ti do projeto), margem default de 512MB.
    VRAMResourceManager mgr(16 * GiB, quiet_policy());

    auto llm = std::make_shared<StubResident>("llm", ResidencyTier::TIER1_HOT, 3 * GiB);
    auto whisper = std::make_shared<StubResident>("whisper", ResidencyTier::TIER2_JIT, 3 * GiB);
    auto tts = std::make_shared<StubResident>("tts", ResidencyTier::TIER2_JIT, 1 * GiB);

    mgr.register_resident(llm);
    mgr.register_resident(whisper);
    mgr.register_resident(tts);

    std::atomic<int> status_events{0};
    mgr.set_resident_status_callback(
        [&](std::string_view, ResidentStatus, ResidentStatus) { ++status_events; });

    std::atomic<int> state_transitions{0};
    mgr.set_state_transition_callback(
        [&](AlyssaState, AlyssaState) { ++state_transitions; });

    // --- Setup: LLM residente (como após CoreIntegration::initialize) --------
    std::cout << "\n[1] LLM residente (TIER1_HOT)" << std::endl;
    CHECK(mgr.current_state() == AlyssaState::IDLE, "estado inicial é IDLE");
    CHECK(llm->load_async().get(), "load do LLM resolve true");
    CHECK(llm->status() == ResidentStatus::RESIDENT, "LLM RESIDENT");
    CHECK(mgr.used_vram_bytes() == 3 * GiB, "used_vram contabiliza o LLM");

    // --- Idempotência ---------------------------------------------------------
    std::cout << "\n[2] Idempotência" << std::endl;
    auto f1 = llm->load_async();
    auto f2 = llm->load_async();
    CHECK(f1.get() && f2.get(), "loads repetidos resolvem true");
    CHECK(llm->load_count.load() == 1, "load em algo RESIDENT não duplica trabalho");
    auto u0 = whisper->unload_async();
    CHECK(u0.get() && whisper->unload_count.load() == 0,
          "unload em algo UNLOADED é no-op");

    // --- Fase 1: ciclo completo da máquina de estados --------------------------
    std::cout << "\n[3] IDLE -> LISTENING -> THINKING -> SPEAKING -> IDLE" << std::endl;

    CHECK(mgr.request_state(AlyssaState::LISTENING).get(),
          "LISTENING resolve quando Whisper está RESIDENT");
    CHECK(whisper->status() == ResidentStatus::RESIDENT, "Whisper RESIDENT em LISTENING");
    CHECK(llm->load_count.load() == 1, "LLM intocado na transição");
    CHECK(mgr.current_state() == AlyssaState::LISTENING, "estado atual = LISTENING");

    CHECK(mgr.request_state(AlyssaState::THINKING).get(), "THINKING resolve");
    CHECK(wait_for([&] { return whisper->status() == ResidentStatus::UNLOADED; }),
          "Whisper descarrega ao sair de LISTENING (em paralelo com o THINKING)");

    CHECK(mgr.request_state(AlyssaState::SPEAKING).get(),
          "SPEAKING resolve quando TTS está RESIDENT");
    CHECK(tts->status() == ResidentStatus::RESIDENT, "TTS RESIDENT em SPEAKING");

    CHECK(mgr.request_state(AlyssaState::IDLE).get(), "IDLE resolve");
    CHECK(wait_for([&] { return tts->status() == ResidentStatus::UNLOADED; }),
          "TTS descarrega ao voltar para IDLE");
    CHECK(llm->status() == ResidentStatus::RESIDENT && llm->load_count.load() == 1,
          "ENTREGA FASE 1: LLM permaneceu residente o turno inteiro (sem reload)");
    CHECK(state_transitions.load() == 4, "callback de transição disparou 4x");
    CHECK(status_events.load() > 0, "callbacks de status de resident dispararam");

    // --- Fase 4: prefetch especulativo -----------------------------------------
    std::cout << "\n[4] Prefetch (pipelining)" << std::endl;
    auto fut_thinking = mgr.request_state(AlyssaState::THINKING);
    mgr.prefetch("tts"); // dispara junto com a "inferência"
    CHECK(fut_thinking.get(), "THINKING resolve");
    CHECK(wait_for([&] { return tts->status() == ResidentStatus::RESIDENT; }),
          "prefetch deixou o TTS RESIDENT durante o THINKING");
    const int tts_loads_before = tts->load_count.load();
    CHECK(mgr.request_state(AlyssaState::SPEAKING).get(), "SPEAKING resolve na hora");
    CHECK(tts->load_count.load() == tts_loads_before,
          "SPEAKING não recarregou o TTS (prefetch virou demanda formal)");
    CHECK(mgr.request_state(AlyssaState::IDLE).get(), "volta a IDLE");
    CHECK(wait_for([&] { return tts->status() == ResidentStatus::UNLOADED; }),
          "TTS descarregado de novo em IDLE");

    // cancel_prefetch: prefetch seguido de cancelamento descarrega
    mgr.prefetch("whisper");
    mgr.cancel_prefetch("whisper");
    CHECK(wait_for([&] { return whisper->status() == ResidentStatus::UNLOADED; }),
          "cancel_prefetch descarrega o resident (best effort)");

    // prefetch descartado sob pressão: orçamento apertado demais
    {
        VRAMResourceManager small(4 * GiB, quiet_policy()); // margem 512MB
        auto big = std::make_shared<StubResident>("tts", ResidencyTier::TIER2_JIT, 2 * GiB);
        auto hot = std::make_shared<StubResident>("llm", ResidencyTier::TIER1_HOT, 2 * GiB);
        small.register_resident(big);
        small.register_resident(hot);
        CHECK(hot->load_async().get(), "LLM do cenário apertado carrega");
        small.prefetch("tts"); // 2+2+0.5 > 4 → descarta
        std::this_thread::sleep_for(100ms);
        CHECK(big->load_count.load() == 0, "prefetch descartado sob pressão de VRAM");
    }

    // --- Fase 2: throttle de vision ---------------------------------------------
    std::cout << "\n[5] Throttle de Vision" << std::endl;
    std::atomic<int> pause_calls{0};
    std::atomic<bool> vision_paused{false};
    mgr.set_vision_pause_hook([&](bool paused) {
        vision_paused = paused;
        ++pause_calls;
    });
    mgr.set_vision_throttle_enabled(true);
    CHECK(mgr.vision_throttle_enabled(), "throttle habilitado");
    CHECK(!vision_paused.load(), "IDLE: vision segue rodando");

    CHECK(mgr.request_state(AlyssaState::LISTENING).get(), "vai para LISTENING");
    CHECK(vision_paused.load(), "LISTENING: vision pausado");
    CHECK(mgr.request_state(AlyssaState::THINKING).get(), "vai para THINKING");
    CHECK(!vision_paused.load(), "THINKING: vision retomado");
    CHECK(mgr.request_state(AlyssaState::SPEAKING).get(), "vai para SPEAKING");
    CHECK(vision_paused.load(), "SPEAKING: vision pausado");
    mgr.set_vision_throttle_enabled(false);
    CHECK(!vision_paused.load(), "desligar o throttle nunca deixa vision pausado");
    CHECK(mgr.request_state(AlyssaState::IDLE).get(), "volta a IDLE");
    const int calls_after_disable = pause_calls.load();
    auto f_listen = mgr.request_state(AlyssaState::LISTENING);
    CHECK(f_listen.get() && pause_calls.load() == calls_after_disable,
          "throttle desligado: hook não dispara mais");
    CHECK(mgr.request_state(AlyssaState::IDLE).get(), "IDLE final");

    // --- Introspecção -----------------------------------------------------------
    std::cout << "\n[6] Introspecção" << std::endl;
    auto ids = mgr.resident_ids();
    CHECK(ids.size() == 3 && ids[0] == "llm" && ids[1] == "tts" && ids[2] == "whisper",
          "resident_ids lista os 3 registrados");
    CHECK(mgr.status_of("llm").value_or(ResidentStatus::FAILED) == ResidentStatus::RESIDENT,
          "status_of(llm) = RESIDENT");
    CHECK(!mgr.status_of("nao_existe").has_value(), "status_of de id desconhecido = nullopt");
    CHECK(mgr.total_vram_budget_bytes() == 16 * GiB, "orçamento reportado");
    CHECK(wait_for([&] { return mgr.used_vram_bytes() == 3 * GiB; }),
          "used_vram volta a só o LLM em IDLE");

    // --- Fase 5: pressão externa via probe --------------------------------------
    std::cout << "\n[7] Fase 5: fits com VRAM real (probe fake)" << std::endl;
    {
        // Orçamento contábil folgado, mas a "GPU real" (fake) está cheia —
        // é o cenário do incidente 2026-07-04 (jogo comendo a VRAM).
        VRAMResourceManager m(16 * GiB, quiet_policy());
        std::atomic<std::size_t> fake_free{12 * GiB};
        m.set_gpu_memory_probe([&]() -> std::optional<GpuMemInfo> {
            const std::size_t f = fake_free.load();
            return GpuMemInfo{16 * GiB, f, 16 * GiB - f};
        });
        auto jit = std::make_shared<StubResident>("tts", ResidencyTier::TIER2_JIT, 1 * GiB);
        m.register_resident(jit);

        CHECK(m.real_free_vram_bytes().value_or(0) == 12 * GiB,
              "real_free_vram_bytes lê o probe");
        fake_free = 1 * GiB; // externo comeu a GPU: 1GiB < 1GiB + margem
        std::this_thread::sleep_for(300ms); // expira o cache do probe (250ms)
        m.prefetch("tts");
        std::this_thread::sleep_for(100ms);
        CHECK(jit->load_count.load() == 0,
              "ENTREGA FASE 5: prefetch descartado por pressão EXTERNA (probe)");
    }

    // --- Fase 5: eviction em cascata com reload garantido ------------------------
    std::cout << "\n[8] Fase 5: eviction de TIER1 + reload garantido" << std::endl;
    {
        VRAMResourceManager m(8 * GiB, quiet_policy());
        auto hot_a = std::make_shared<StubResident>("llm", ResidencyTier::TIER1_HOT, 3 * GiB);
        auto hot_b = std::make_shared<StubResident>("vision", ResidencyTier::TIER1_HOT, 4 * GiB);
        auto jit = std::make_shared<StubResident>("whisper", ResidencyTier::TIER2_JIT, 3 * GiB);
        m.register_resident(hot_a);
        m.register_resident(hot_b);
        m.register_resident(jit);
        CHECK(hot_a->load_async().get() && hot_b->load_async().get(),
              "dois HOTs residentes (7GiB de 8GiB)");

        // LISTENING exige o whisper (3GiB): 7+3+0.5 > 8 → eviction. A vítima
        // certa é o hot_b (maior reclaimable cobre o deficit sozinho).
        CHECK(m.request_state(AlyssaState::LISTENING).get(),
              "LISTENING resolve mesmo sem espaço (eviction abriu caminho)");
        CHECK(jit->status() == ResidentStatus::RESIDENT, "whisper RESIDENT pós-eviction");
        CHECK(hot_b->status() == ResidentStatus::UNLOADED,
              "ENTREGA FASE 5: TIER1 evitado sob pressão (maior primeiro)");
        CHECK(hot_a->status() == ResidentStatus::RESIDENT,
              "o outro HOT não foi tocado (eviction mínima)");

        // Sai de LISTENING: whisper descarrega; com espaço de volta, o HOT
        // evitado recarrega (garantia de reload da política).
        CHECK(m.request_state(AlyssaState::THINKING).get(), "THINKING resolve");
        CHECK(wait_for([&] { return jit->status() == ResidentStatus::UNLOADED; }),
              "whisper saiu");
        CHECK(m.request_state(AlyssaState::IDLE).get(), "IDLE resolve");
        CHECK(wait_for([&] { return hot_b->status() == ResidentStatus::RESIDENT; }),
              "ENTREGA FASE 5: HOT evitado recarregou quando coube de novo");
    }

    // --- Fase 5: unload recusado não trava o scheduler ---------------------------
    std::cout << "\n[9] Fase 5: eviction com unload recusado (ex.: LLM)" << std::endl;
    {
        // Resident que recusa unload (como o LLMResident real até o
        // CoreIntegration suportar reload).
        class Refuser : public StubResident {
        public:
            using StubResident::StubResident;
        protected:
            bool do_unload() override { return false; }
        };
        VRAMResourceManager m(6 * GiB, quiet_policy());
        auto stubborn = std::make_shared<Refuser>("llm", ResidencyTier::TIER1_HOT, 3 * GiB);
        auto jit = std::make_shared<StubResident>("whisper", ResidencyTier::TIER2_JIT, 3 * GiB);
        m.register_resident(stubborn);
        m.register_resident(jit);
        CHECK(stubborn->load_async().get(), "HOT teimoso residente");

        auto fut = m.request_state(AlyssaState::LISTENING);
        CHECK(fut.get(), "request_state resolve mesmo com eviction recusada");
        CHECK(stubborn->status() == ResidentStatus::RESIDENT,
              "unload recusado → HOT continua RESIDENT (sem estado inconsistente)");
        CHECK(jit->status() == ResidentStatus::RESIDENT,
              "o load da demanda ainda foi tentado (backend decide se cabe)");
        CHECK(m.request_state(AlyssaState::IDLE).get(), "volta a IDLE");
    }

    // --- Fase 5: watchdog de pressão com histerese --------------------------------
    std::cout << "\n[10] Fase 5: watchdog de pressão (probe fake)" << std::endl;
    {
        EvictionPolicy p;
        p.use_gpu_probe = false;        // probe entra por injeção
        p.pressure_poll_ms = 30;        // watchdog rápido p/ teste
        p.pressure_floor_bytes = 2 * GiB;
        p.rearm_factor = 2.0;           // rearma com >= 4GiB livres
        VRAMResourceManager m(16 * GiB, p);

        std::atomic<std::size_t> fake_free{8 * GiB};
        m.set_gpu_memory_probe([&]() -> std::optional<GpuMemInfo> {
            const std::size_t f = fake_free.load();
            return GpuMemInfo{16 * GiB, f, 16 * GiB - f};
        });

        auto hot = std::make_shared<StubResident>("llm", ResidencyTier::TIER1_HOT, 3 * GiB);
        auto jit = std::make_shared<StubResident>("tts", ResidencyTier::TIER2_JIT, 1 * GiB);
        m.register_resident(hot);
        m.register_resident(jit);
        CHECK(hot->load_async().get() && jit->load_async().get(), "HOT e JIT residentes");

        std::atomic<int> pressure_events{0};
        m.set_pressure_callback([&](std::size_t, std::size_t) { ++pressure_events; });

        fake_free = 1 * GiB; // usuário abriu o jogo no meio da sessão
        CHECK(wait_for([&] { return jit->status() == ResidentStatus::UNLOADED; }),
              "ENTREGA FASE 5: watchdog descarregou o JIT ocioso sob pressão");
        CHECK(wait_for([&] { return hot->status() == ResidentStatus::UNLOADED; }),
              "watchdog evitou o HOT (allow_hot_tier_eviction)");
        CHECK(wait_for([&] { return pressure_events.load() >= 1; }),
              "callback de pressão disparou");
        std::this_thread::sleep_for(200ms); // vários ticks ainda sob pressão
        CHECK(pressure_events.load() == 1,
              "callback é edge-triggered (não spamma a cada tick)");

        fake_free = 3 * GiB; // saiu da pressão mas AINDA sem folga p/ rearm
        std::this_thread::sleep_for(200ms);
        CHECK(hot->status() == ResidentStatus::UNLOADED,
              "histerese: 3GiB livres < 4GiB de rearm → HOT segue fora");

        fake_free = 8 * GiB; // jogo fechado
        CHECK(wait_for([&] { return hot->status() == ResidentStatus::RESIDENT; }),
              "ENTREGA FASE 5: folga rearmou → HOT recarregado sozinho");
        CHECK(jit->status() == ResidentStatus::UNLOADED,
              "JIT não recarrega no rearm (ele é sob demanda por definição)");

        fake_free = 1 * GiB;
        CHECK(wait_for([&] { return pressure_events.load() == 2; }),
              "nova entrada em pressão → novo evento (edge de novo)");
    }

    // --- Fase 5: probe real (smoke, só informativo) -------------------------------
    std::cout << "\n[11] Fase 5: probe real (NVML/nvidia-smi)" << std::endl;
    {
        const auto real = query_gpu_memory();
        if (real) {
            std::cout << "  GPU real: total " << (real->total_bytes >> 20)
                      << " MiB, livre " << (real->free_bytes >> 20)
                      << " MiB, uso global " << (real->used_bytes >> 20)
                      << " MiB" << std::endl;
            CHECK(real->total_bytes > 0 && real->free_bytes <= real->total_bytes,
                  "probe real retorna números consistentes");
        } else {
            std::cout << "  (sem GPU NVIDIA detectável — probe indisponível, ok)"
                      << std::endl;
        }
    }

    std::cout << "\n=== " << (g_failures == 0 ? "TODOS OS TESTES PASSARAM"
                                              : std::to_string(g_failures) + " FALHA(S)")
              << " ===" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
