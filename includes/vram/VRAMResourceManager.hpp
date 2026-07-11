// =============================================================================
// VRAMResourceManager.hpp
// AlyssaNet (Reborn) — Subsistema de gerenciamento de VRAM por tiers
//
// Responsabilidade: orquestrar quais modelos residem na VRAM em cada
// instante (Whisper, LLM/Gemma, TTS, contexto CUDA do OpenCV), minimizando
// latência percebida via pré-carregamento especulativo e pipelining de
// load/unload assíncrono.
//
// Interface conforme plano_scheduler/VRAMResourceManager.hpp, com um adendo
// da Fase 2: set_vision_pause_hook() — o manager não conhece o pipeline de
// vision, só avisa "pausa/retoma" para quem registrar o hook.
// =============================================================================

#pragma once

#include "GpuMemoryProbe.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace alyssa::vram {

// -----------------------------------------------------------------------------
// Tiers de residência
// -----------------------------------------------------------------------------
// TIER0_PERMANENT — nunca descarrega (ex: contexto CUDA do OpenCV).
// TIER1_HOT       — fica residente, só cede espaço sob pressão explícita
//                    (ex: LLM/Gemma — caro de recarregar).
// TIER2_JIT       — carrega sob demanda e descarrega assim que possível
//                    (ex: Whisper, TTS — nunca competem entre si).
enum class ResidencyTier : uint8_t {
    TIER0_PERMANENT = 0,
    TIER1_HOT       = 1,
    TIER2_JIT       = 2,
};

// -----------------------------------------------------------------------------
// Estados globais da máquina de estados da Alyssa
// -----------------------------------------------------------------------------
enum class AlyssaState : uint8_t {
    IDLE,        // LLM residente + OpenCV context. Nada mais na VRAM.
    LISTENING,   // Whisper sendo carregado/ativo.
    THINKING,    // LLM processando (já residente, sem load extra).
    SPEAKING,    // TTS sendo carregado/ativo.
};

[[nodiscard]] constexpr std::string_view to_string(AlyssaState s) noexcept {
    switch (s) {
        case AlyssaState::IDLE:      return "IDLE";
        case AlyssaState::LISTENING: return "LISTENING";
        case AlyssaState::THINKING:  return "THINKING";
        case AlyssaState::SPEAKING:  return "SPEAKING";
    }
    return "UNKNOWN";
}

// -----------------------------------------------------------------------------
// Status de um recurso residente
// -----------------------------------------------------------------------------
enum class ResidentStatus : uint8_t {
    UNLOADED,     // Não está em VRAM (pode estar em RAM, pronto pra JIT).
    LOADING,      // Transferência RAM -> VRAM em andamento.
    RESIDENT,     // Totalmente carregado e utilizável.
    UNLOADING,    // Sendo descarregado.
    FAILED,       // Última operação de load/unload falhou.
};

[[nodiscard]] constexpr std::string_view to_string(ResidentStatus s) noexcept {
    switch (s) {
        case ResidentStatus::UNLOADED:  return "UNLOADED";
        case ResidentStatus::LOADING:   return "LOADING";
        case ResidentStatus::RESIDENT:  return "RESIDENT";
        case ResidentStatus::UNLOADING: return "UNLOADING";
        case ResidentStatus::FAILED:    return "FAILED";
    }
    return "UNKNOWN";
}

// -----------------------------------------------------------------------------
// Métricas expostas por cada recurso (usadas pelo manager para decisões
// de scheduling e para telemetria/log).
// -----------------------------------------------------------------------------
struct ResidentMetrics {
    std::size_t   vram_bytes_footprint = 0;      // Pegada estimada em VRAM.
    std::chrono::milliseconds avg_load_time{0};   // Média móvel de load.
    std::chrono::milliseconds avg_unload_time{0}; // Média móvel de unload.
    std::chrono::steady_clock::time_point last_used{};
};

// -----------------------------------------------------------------------------
// IVRAMResident — contrato que todo backend (Whisper, LLM, TTS, Vision)
// precisa implementar para ser gerenciável pelo VRAMResourceManager.
// -----------------------------------------------------------------------------
class IVRAMResident {
public:
    virtual ~IVRAMResident() = default;

    // Identificador estável, usado em logs e no mapa interno do manager.
    [[nodiscard]] virtual std::string_view id() const noexcept = 0;

    [[nodiscard]] virtual ResidencyTier tier() const noexcept = 0;

    [[nodiscard]] virtual ResidentStatus status() const noexcept = 0;

    [[nodiscard]] virtual const ResidentMetrics& metrics() const noexcept = 0;

    // Carrega o modelo para VRAM. DEVE ser assíncrono/não bloqueante:
    // retorna imediatamente um future que resolve quando RESIDENT.
    // Implementações devem ser idempotentes (chamar load() em algo já
    // RESIDENT ou LOADING não deve duplicar trabalho).
    [[nodiscard]] virtual std::future<bool> load_async() = 0;

    // Descarrega da VRAM. Também assíncrono e idempotente.
    [[nodiscard]] virtual std::future<bool> unload_async() = 0;

    // Estimativa de bytes que SERIAM liberados caso unload() seja chamado
    // agora. Usado pelo manager para decidir de quem "roubar" espaço
    // quando TIER1_HOT precisa abrir espaço para um TIER2_JIT.
    [[nodiscard]] virtual std::size_t reclaimable_bytes() const noexcept = 0;
};

using ResidentPtr = std::shared_ptr<IVRAMResident>;

// -----------------------------------------------------------------------------
// Política de pressão de VRAM: decide o que fazer quando não há espaço
// livre suficiente para satisfazer um load pedido.
// -----------------------------------------------------------------------------
struct EvictionPolicy {
    // Margem de segurança que o manager tenta manter livre (bytes),
    // para absorver picos de alocação temporária de kernels CUDA.
    std::size_t safety_margin_bytes = 512ull * 1024 * 1024; // 512MB default

    // Se true, o manager pode descarregar temporariamente um recurso
    // TIER1_HOT para abrir espaço para um TIER2_JIT urgente, desde que
    // haja garantia de reload assim que o JIT terminar.
    bool allow_hot_tier_eviction = true;

    // ------------------------- Fase 5 ---------------------------------
    // Consciência de VRAM *real* (incidente 2026-07-04: o desktop come o
    // mesmo orçamento — jogo aberto = 6.3GiB que o manager não via).

    // fits() passa a exigir folga na VRAM livre REAL da GPU (via probe),
    // além da contabilidade própria. Sem probe disponível, degrada para
    // contabilidade própria (comportamento pré-Fase 5).
    bool use_gpu_probe = true;

    // Cache da leitura do probe dentro de decisões de scheduling (evita
    // re-consultar a NVML várias vezes na mesma transição).
    std::uint32_t probe_cache_ms = 250;

    // Watchdog de pressão: amostra a VRAM real a cada N ms e reage se a
    // folga cair abaixo do piso (mesmo sem nenhuma transição de estado —
    // ex.: usuário abriu um jogo no meio da sessão). 0 = desligado.
    std::uint32_t pressure_poll_ms = 1000;

    // Folga real mínima. Abaixo disso o watchdog: descarta prefetches,
    // descarrega JITs ociosos e (se allow_hot_tier_eviction) evita HOTs.
    std::size_t pressure_floor_bytes = 1536ull * 1024 * 1024; // 1.5GiB

    // Histerese do rearm: HOTs evitados só recarregam quando a folga real
    // volta acima de pressure_floor_bytes * rearm_factor.
    double rearm_factor = 2.0;
};

// Fase 5: notificação de pressão de VRAM (edge-triggered: dispara ao ENTRAR
// em pressão, não a cada tick). Uso pretendido: EndocrineSystem (cortisol),
// log, UI. O callback NÃO deve chamar métodos do manager sincronamente.
using PressureCallback =
    std::function<void(std::size_t free_bytes, std::size_t floor_bytes)>;

// -----------------------------------------------------------------------------
// Callback de telemetria — o manager notifica transições de estado para
// que o EndocrineSystem / logging / UI possam reagir (ex: hesitação
// visível enquanto um load demorado acontece).
// -----------------------------------------------------------------------------
using StateTransitionCallback =
    std::function<void(AlyssaState from, AlyssaState to)>;

using ResidentStatusCallback =
    std::function<void(std::string_view resident_id,
                        ResidentStatus old_status,
                        ResidentStatus new_status)>;

// -----------------------------------------------------------------------------
// VRAMResourceManager — orquestrador central.
// -----------------------------------------------------------------------------
// Thread-safety: todos os métodos públicos são seguros para chamada
// concorrente. O manager serializa decisões de scheduling internamente
// via mutex; as operações de load/unload em si rodam em threads/streams
// separadas (não bloqueiam o chamador).
// -----------------------------------------------------------------------------
class VRAMResourceManager {
public:
    explicit VRAMResourceManager(std::size_t total_vram_budget_bytes,
                                  EvictionPolicy policy = {});

    ~VRAMResourceManager();

    VRAMResourceManager(const VRAMResourceManager&)            = delete;
    VRAMResourceManager& operator=(const VRAMResourceManager&) = delete;

    // -------------------------------------------------------------------
    // Registro de recursos
    // -------------------------------------------------------------------
    void register_resident(ResidentPtr resident);
    void unregister_resident(std::string_view id);

    // -------------------------------------------------------------------
    // Transições de estado de alto nível. Cada uma dispara internamente
    // as chamadas de load_async/unload_async necessárias nos residents
    // registrados, respeitando tiers e pipelining.
    //
    // Ex.: request_state(LISTENING) vai:
    //   1. Disparar load_async() do resident TIER2_JIT com id "whisper"
    //   2. NÃO tocar no LLM (TIER1_HOT permanece residente)
    //   3. Retornar future que resolve quando Whisper estiver RESIDENT
    // -------------------------------------------------------------------
    [[nodiscard]] std::future<bool> request_state(AlyssaState next);

    [[nodiscard]] AlyssaState current_state() const noexcept;

    // -------------------------------------------------------------------
    // Pré-carregamento especulativo. Usado para pipelining: por exemplo,
    // assim que o STT emite o texto final, o pipeline já pode chamar
    // prefetch("tts") em paralelo com a inferência do LLM, para que o
    // TTS já esteja RESIDENT quando os primeiros tokens chegarem.
    //
    // Não muda o AlyssaState atual — é uma dica de otimização, não uma
    // transição formal. Se não houver orçamento de VRAM disponível, o
    // prefetch é enfileirado e pode ser descartado sob pressão.
    // -------------------------------------------------------------------
    void prefetch(std::string_view resident_id);
    void cancel_prefetch(std::string_view resident_id);

    // -------------------------------------------------------------------
    // Introspecção / telemetria
    // -------------------------------------------------------------------
    [[nodiscard]] std::size_t total_vram_budget_bytes() const noexcept;
    [[nodiscard]] std::size_t used_vram_bytes() const noexcept;
    [[nodiscard]] std::size_t free_vram_bytes() const noexcept;

    // Fase 5: folga REAL da GPU (via probe). nullopt = probe indisponível.
    [[nodiscard]] std::optional<std::size_t> real_free_vram_bytes() const;

    // Fase 5: troca a fonte da verdade de VRAM (default: GpuMemoryProbe/NVML
    // quando policy.use_gpu_probe). Testes injetam um probe fake aqui.
    void set_gpu_memory_probe(std::function<std::optional<GpuMemInfo>()> probe);

    // Fase 5: callback de entrada em pressão (ver PressureCallback).
    void set_pressure_callback(PressureCallback cb);

    [[nodiscard]] std::optional<ResidentStatus>
        status_of(std::string_view resident_id) const noexcept;

    [[nodiscard]] std::vector<std::string> resident_ids() const;

    void set_state_transition_callback(StateTransitionCallback cb);
    void set_resident_status_callback(ResidentStatusCallback cb);

    // -------------------------------------------------------------------
    // Throttle de Vision (OpenCV/CUDA) — ver plano, fase 2.
    // Pausa/retoma o pipeline de vision em função da pressão de VRAM
    // e do AlyssaState atual (ex: pausar captura durante SPEAKING).
    //
    // O manager não conhece o pipeline de vision: quem o possui registra
    // o hook (chamado com true = pausar, false = retomar). Com o throttle
    // habilitado, o hook dispara nas transições para LISTENING/SPEAKING
    // (pausa) e IDLE/THINKING (retoma).
    // -------------------------------------------------------------------
    void set_vision_throttle_enabled(bool enabled);
    [[nodiscard]] bool vision_throttle_enabled() const noexcept;
    void set_vision_pause_hook(std::function<void(bool paused)> hook);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alyssa::vram
