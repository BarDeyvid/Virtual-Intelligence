// =============================================================================
// ResidentBase.hpp — maquinaria comum de IVRAMResident (header-only)
//
// Todo backend concreto (Whisper, LLM, TTS) só precisa implementar
// do_load()/do_unload() bloqueantes; a base cuida de:
//   - assincronia real (thread destacada + promise → o future retornado
//     NÃO bloqueia no destrutor, diferente de std::async)
//   - idempotência (load em algo RESIDENT/LOADING não duplica trabalho)
//   - métricas (média móvel de load/unload, last_used)
//   - notificação de mudança de status (ResidentStatusCallback)
//
// Semântica de falha: load que falha → FAILED (pode tentar de novo);
// unload que falha → volta a RESIDENT (falhou em sair ≠ deixou de existir).
// =============================================================================

#pragma once

#include "VRAMResourceManager.hpp"

#include <exception>
#include <iostream>
#include <thread>
#include <utility>

namespace alyssa::vram {

class ResidentBase : public IVRAMResident {
public:
    ResidentBase(std::string id, ResidencyTier tier, std::size_t footprint_bytes)
        : id_(std::move(id)), tier_(tier) {
        metrics_.vram_bytes_footprint = footprint_bytes;
    }

    [[nodiscard]] std::string_view id() const noexcept override { return id_; }
    [[nodiscard]] ResidencyTier tier() const noexcept override { return tier_; }
    [[nodiscard]] ResidentStatus status() const noexcept override {
        return status_.load(std::memory_order_acquire);
    }
    [[nodiscard]] const ResidentMetrics& metrics() const noexcept override {
        return metrics_;
    }
    [[nodiscard]] std::size_t reclaimable_bytes() const noexcept override {
        return status() == ResidentStatus::RESIDENT ? metrics_.vram_bytes_footprint : 0;
    }

    /// Manager registra aqui seu ResidentStatusCallback (via register_resident).
    void set_status_listener(ResidentStatusCallback cb) {
        std::lock_guard<std::mutex> lk(state_mtx_);
        listener_ = std::move(cb);
    }

    [[nodiscard]] std::future<bool> load_async() override {
        std::lock_guard<std::mutex> lk(state_mtx_);
        const ResidentStatus s = status_.load();
        if (s == ResidentStatus::RESIDENT) return ready(true);
        if (s == ResidentStatus::LOADING && inflight_.valid()) return wrap(inflight_);

        transition_locked(ResidentStatus::LOADING);
        auto prom = std::make_shared<std::promise<bool>>();
        inflight_ = prom->get_future().share();

        std::thread([this, prom] {
            const auto t0 = std::chrono::steady_clock::now();
            bool ok = false;
            try {
                std::lock_guard<std::mutex> op(op_mtx_);
                ok = do_load();
            } catch (const std::exception& e) {
                std::cerr << "[vram] load de '" << id_ << "' lançou: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[vram] load de '" << id_ << "' lançou exceção desconhecida" << std::endl;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0);
            {
                std::lock_guard<std::mutex> lk2(state_mtx_);
                if (ok) update_moving_avg(metrics_.avg_load_time, elapsed);
                metrics_.last_used = std::chrono::steady_clock::now();
                // Se um unload chegou enquanto o load rodava, o status já é
                // UNLOADING — quem finaliza é o worker do unload, não nós.
                if (status_.load() == ResidentStatus::LOADING) {
                    transition_locked(ok ? ResidentStatus::RESIDENT : ResidentStatus::FAILED);
                }
            }
            prom->set_value(ok);
        }).detach();

        return wrap(inflight_);
    }

    [[nodiscard]] std::future<bool> unload_async() override {
        std::lock_guard<std::mutex> lk(state_mtx_);
        const ResidentStatus s = status_.load();
        if (s == ResidentStatus::UNLOADED || s == ResidentStatus::FAILED) return ready(true);
        if (s == ResidentStatus::UNLOADING && inflight_.valid()) return wrap(inflight_);

        // Se um load está em andamento, espera ele terminar antes de descarregar
        // (a thread abaixo bloqueia no op_mtx_/pending até o load resolver).
        std::shared_future<bool> pending_load;
        if (s == ResidentStatus::LOADING && inflight_.valid()) pending_load = inflight_;

        transition_locked(ResidentStatus::UNLOADING);
        auto prom = std::make_shared<std::promise<bool>>();
        inflight_ = prom->get_future().share();

        std::thread([this, prom, pending_load] {
            if (pending_load.valid()) pending_load.wait();
            const auto t0 = std::chrono::steady_clock::now();
            bool ok = false;
            try {
                std::lock_guard<std::mutex> op(op_mtx_);
                ok = do_unload();
            } catch (const std::exception& e) {
                std::cerr << "[vram] unload de '" << id_ << "' lançou: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[vram] unload de '" << id_ << "' lançou exceção desconhecida" << std::endl;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0);
            {
                std::lock_guard<std::mutex> lk2(state_mtx_);
                if (ok) update_moving_avg(metrics_.avg_unload_time, elapsed);
                // Unload que falha: o recurso continua na VRAM.
                transition_locked(ok ? ResidentStatus::UNLOADED : ResidentStatus::RESIDENT);
            }
            prom->set_value(ok);
        }).detach();

        return wrap(inflight_);
    }

protected:
    /// Load bloqueante do backend. true = sucesso. Roda em thread própria,
    /// serializado com do_unload() pelo op_mtx_.
    virtual bool do_load() = 0;

    /// Unload bloqueante do backend. true = sucesso.
    virtual bool do_unload() = 0;

    /// Backends que só conhecem o footprint depois do load (ex: llama_model_size)
    /// atualizam aqui de dentro do do_load().
    void set_footprint(std::size_t bytes) { metrics_.vram_bytes_footprint = bytes; }

private:
    static std::future<bool> ready(bool value) {
        std::promise<bool> p;
        p.set_value(value);
        return p.get_future();
    }

    // Future "deferred" sobre o shared_future em andamento: não bloqueia no
    // destrutor e vários chamadores podem esperar a mesma operação.
    static std::future<bool> wrap(std::shared_future<bool> sf) {
        return std::async(std::launch::deferred, [sf] { return sf.get(); });
    }

    static void update_moving_avg(std::chrono::milliseconds& avg,
                                  std::chrono::milliseconds sample) {
        avg = (avg.count() == 0) ? sample : (avg * 3 + sample) / 4;
    }

    // Pré-condição: state_mtx_ travado. Notifica o listener fora não é
    // possível aqui sem soltar o lock; o listener deve ser barato e NÃO
    // chamar métodos do resident/manager sincronamente (só log/UI/post).
    void transition_locked(ResidentStatus next) {
        const ResidentStatus old = status_.exchange(next, std::memory_order_release);
        if (old != next && listener_) listener_(id_, old, next);
    }

    std::string id_;
    ResidencyTier tier_;
    std::atomic<ResidentStatus> status_{ResidentStatus::UNLOADED};
    ResidentMetrics metrics_;
    ResidentStatusCallback listener_;
    std::mutex state_mtx_;             ///< Guarda transições de status/metrics/inflight
    std::mutex op_mtx_;                ///< Serializa do_load/do_unload entre si
    std::shared_future<bool> inflight_; ///< Operação em andamento (load OU unload)
};

// -----------------------------------------------------------------------------
// FunctionResident — resident genérico configurado com lambdas.
// Usado nos testes (stubs) e como ponte para backends simples.
// -----------------------------------------------------------------------------
class FunctionResident : public ResidentBase {
public:
    using Op = std::function<bool()>;

    FunctionResident(std::string id, ResidencyTier tier, std::size_t footprint_bytes,
                     Op loader = {}, Op unloader = {})
        : ResidentBase(std::move(id), tier, footprint_bytes),
          loader_(std::move(loader)), unloader_(std::move(unloader)) {}

protected:
    bool do_load() override { return loader_ ? loader_() : true; }
    bool do_unload() override { return unloader_ ? unloader_() : true; }

private:
    Op loader_;
    Op unloader_;
};

} // namespace alyssa::vram
