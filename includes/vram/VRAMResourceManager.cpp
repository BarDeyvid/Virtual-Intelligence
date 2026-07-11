// =============================================================================
// VRAMResourceManager.cpp — implementação do scheduler de VRAM por tiers.
//
// Fase 1: máquina de estados "burra" (load/unload por transição, sem eviction).
// Fase 2: throttle de vision por estado (hook de pausa).
// Fase 4: prefetch especulativo com descarte sob pressão.
// Fase 5: consciência de VRAM REAL (GpuMemoryProbe/NVML) + eviction em
//         cascata (prefetch → JIT ocioso → TIER1_HOT com reload garantido)
//         + watchdog de pressão com histerese. Motivada pelo incidente de
//         2026-07-04 (desktop externo comeu o orçamento → BSOD): o manager
//         agora reage ao que o RESTO do sistema faz com a GPU.
// =============================================================================

#include "VRAMResourceManager.hpp"
#include "ResidentBase.hpp"

#include <algorithm>
#include <iostream>
#include <thread>
#include <unordered_set>

namespace alyssa::vram {

namespace {

// Qual resident TIER2_JIT cada estado exige. Convenção de ids do plano:
// "whisper" (LISTENING) e "tts" (SPEAKING). IDLE/THINKING não exigem JIT.
const char* jit_id_for(AlyssaState s) {
    switch (s) {
        case AlyssaState::LISTENING: return "whisper";
        case AlyssaState::SPEAKING:  return "tts";
        default:                     return nullptr;
    }
}

bool state_wants_vision_paused(AlyssaState s) {
    return s == AlyssaState::LISTENING || s == AlyssaState::SPEAKING;
}

} // namespace

struct VRAMResourceManager::Impl {
    const std::size_t budget;
    const EvictionPolicy policy;

    mutable std::mutex mtx;
    std::unordered_map<std::string, ResidentPtr> residents;
    AlyssaState state = AlyssaState::IDLE;
    StateTransitionCallback on_state;
    PressureCallback on_pressure;

    // O callback de status tem mutex PRÓPRIO: o listener dispara de dentro de
    // load_async/unload_async, que o request_state chama já segurando mtx —
    // reusar mtx aqui deadlockaria na mesma thread.
    std::mutex status_cb_mtx;
    ResidentStatusCallback on_status;

    bool vision_throttle = false;
    bool vision_paused = false;
    std::function<void(bool)> vision_hook;

    // Prefetches em voo: id → future do load. Consumidos (esquecidos) quando
    // o estado formal passa a exigir o resident, ou cancelados.
    std::unordered_map<std::string, std::shared_future<bool>> prefetches;

    // ------------------------- Fase 5 ---------------------------------
    std::function<std::optional<GpuMemInfo>()> probe;
    mutable std::optional<GpuMemInfo> probe_cache;
    mutable std::chrono::steady_clock::time_point probe_stamp{};

    // HOTs evitados sob pressão — recarregam quando a folga real rearma
    // (garantia de reload da EvictionPolicy).
    std::unordered_set<std::string> hot_evicted;
    bool pressured = false;

    std::thread watchdog;
    std::atomic<bool> stop_watchdog{false};

    Impl(std::size_t b, EvictionPolicy p) : budget(b), policy(p) {}

    // Pré-condição: mtx travado. Soma o footprint de tudo que ocupa (ou está
    // entrando na) VRAM agora.
    std::size_t used_locked() const {
        std::size_t used = 0;
        for (const auto& [id, r] : residents) {
            const ResidentStatus s = r->status();
            if (s == ResidentStatus::RESIDENT || s == ResidentStatus::LOADING ||
                s == ResidentStatus::UNLOADING) {
                used += r->metrics().vram_bytes_footprint;
            }
        }
        return used;
    }

    // Pré-condição: mtx travado. Leitura da VRAM real com cache curto (uma
    // transição de estado não precisa consultar a NVML três vezes).
    std::optional<GpuMemInfo> probe_locked() const {
        if (!probe) return std::nullopt;
        const auto now = std::chrono::steady_clock::now();
        if (probe_cache &&
            now - probe_stamp < std::chrono::milliseconds(policy.probe_cache_ms)) {
            return probe_cache;
        }
        probe_cache = probe();
        probe_stamp = now;
        return probe_cache;
    }

    // Pré-condição: mtx travado. true = cabe no orçamento contábil E na
    // VRAM livre REAL (quando há probe), ambos com margem de segurança.
    bool fits_locked(std::size_t incoming_bytes) const {
        const std::size_t used = used_locked();
        if (used + incoming_bytes + policy.safety_margin_bytes > budget) return false;
        if (const auto mem = probe_locked()) {
            if (mem->free_bytes < incoming_bytes + policy.safety_margin_bytes)
                return false;
        }
        return true;
    }

    // Pré-condição: mtx travado. Quantos bytes faltam para 'incoming' caber
    // com margem (0 = já cabe). Considera o pior dos dois sinais.
    std::size_t deficit_locked(std::size_t incoming_bytes) const {
        const std::size_t target = incoming_bytes + policy.safety_margin_bytes;
        std::size_t deficit = 0;
        const std::size_t used = used_locked();
        if (used + target > budget) deficit = used + target - budget;
        if (const auto mem = probe_locked()) {
            if (mem->free_bytes < target)
                deficit = std::max(deficit, target - mem->free_bytes);
        }
        return deficit;
    }

    // Pré-condição: mtx travado. Seleciona vítimas de eviction até cobrir o
    // deficit: JIT ociosos primeiro, depois TIER1_HOT (se a política deixa),
    // maiores reclaimable primeiro. TIER0 e o resident protegido nunca.
    // HOTs escolhidos entram em hot_evicted (contrato de reload garantido).
    std::vector<ResidentPtr> pick_victims_locked(std::size_t deficit,
                                                 std::string_view protect_id) {
        std::vector<ResidentPtr> victims;
        auto take_pass = [&](ResidencyTier tier) {
            std::vector<ResidentPtr> pass;
            for (auto& [id, r] : residents) {
                if (r->tier() != tier || id == protect_id) continue;
                if (r->reclaimable_bytes() == 0) continue;
                pass.push_back(r);
            }
            std::sort(pass.begin(), pass.end(), [](const auto& a, const auto& b) {
                return a->reclaimable_bytes() > b->reclaimable_bytes();
            });
            for (auto& r : pass) {
                if (deficit == 0) break;
                const std::size_t got = r->reclaimable_bytes();
                deficit -= std::min(deficit, got);
                if (tier == ResidencyTier::TIER1_HOT)
                    hot_evicted.insert(std::string(r->id()));
                victims.push_back(std::move(r));
            }
        };
        take_pass(ResidencyTier::TIER2_JIT);
        if (deficit > 0 && policy.allow_hot_tier_eviction)
            take_pass(ResidencyTier::TIER1_HOT);
        if (deficit > 0) {
            std::cerr << "[vram] eviction insuficiente: ainda faltam "
                      << (deficit >> 20) << " MiB depois de evitar tudo que dava"
                      << std::endl;
        }
        return victims;
    }

    // Pré-condição: mtx travado. Aplica o throttle de vision para o estado.
    // Retorna o hook a chamar (fora do lock) ou nullptr se nada muda.
    std::function<void(bool)> vision_action_locked(AlyssaState next, bool& pause_out) {
        if (!vision_throttle || !vision_hook) return nullptr;
        const bool want_paused = state_wants_vision_paused(next);
        if (want_paused == vision_paused) return nullptr;
        vision_paused = want_paused;
        pause_out = want_paused;
        return vision_hook;
    }

    // Corpo do watchdog de pressão (Fase 5). Roda fora de qualquer lock; toma
    // mtx só para decidir, executa unload/reload/callbacks depois de soltar.
    void watchdog_tick() {
        std::function<std::optional<GpuMemInfo>()> local_probe;
        {
            std::lock_guard<std::mutex> lk(mtx);
            local_probe = probe;
        }
        if (!local_probe) return;
        const auto mem = local_probe();
        if (!mem) return;

        std::vector<ResidentPtr> to_unload;
        std::vector<ResidentPtr> to_reload;
        PressureCallback cb;
        bool entering = false;
        {
            std::lock_guard<std::mutex> lk(mtx);
            probe_cache = mem; // watchdog renova o cache de graça
            probe_stamp = std::chrono::steady_clock::now();

            const std::size_t floor = policy.pressure_floor_bytes;
            const char* wanted = jit_id_for(state);

            if (mem->free_bytes < floor) {
                entering = !pressured;
                pressured = true;

                // 1) Prefetches viram alvo: eram só otimização.
                for (auto it = prefetches.begin(); it != prefetches.end();) {
                    if (!wanted || it->first != wanted) {
                        auto rit = residents.find(it->first);
                        if (rit != residents.end()) to_unload.push_back(rit->second);
                        it = prefetches.erase(it);
                    } else {
                        ++it;
                    }
                }
                // 2) JITs ociosos saem; 3) HOTs saem se a política deixa.
                for (auto& [id, r] : residents) {
                    if (wanted && id == wanted) continue;
                    if (r->reclaimable_bytes() == 0) continue;
                    if (r->tier() == ResidencyTier::TIER2_JIT) {
                        to_unload.push_back(r);
                    } else if (r->tier() == ResidencyTier::TIER1_HOT &&
                               policy.allow_hot_tier_eviction) {
                        hot_evicted.insert(id);
                        to_unload.push_back(r);
                    }
                }
                if (entering) cb = on_pressure;
            } else {
                pressured = false;
                // Rearm com histerese: só recarrega HOTs com folga confortável.
                const auto rearm = static_cast<std::size_t>(
                    static_cast<double>(floor) * policy.rearm_factor);
                if (!hot_evicted.empty() && mem->free_bytes >= rearm) {
                    for (auto it = hot_evicted.begin(); it != hot_evicted.end();) {
                        auto rit = residents.find(*it);
                        if (rit != residents.end()) to_reload.push_back(rit->second);
                        it = hot_evicted.erase(it);
                    }
                }
            }
        }

        for (auto& r : to_unload) {
            std::cout << "[vram] pressão: descarregando '" << r->id() << "'" << std::endl;
            auto f = r->unload_async();
            (void)f;
        }
        for (auto& r : to_reload) {
            std::cout << "[vram] folga rearmada: recarregando '" << r->id() << "'" << std::endl;
            auto f = r->load_async();
            (void)f;
        }
        if (cb) {
            std::cout << "[vram] PRESSÃO de VRAM: livre "
                      << (mem->free_bytes >> 20) << " MiB < piso "
                      << (policy.pressure_floor_bytes >> 20) << " MiB" << std::endl;
            cb(mem->free_bytes, policy.pressure_floor_bytes);
        }
    }
};

VRAMResourceManager::VRAMResourceManager(std::size_t total_vram_budget_bytes,
                                         EvictionPolicy policy)
    : impl_(std::make_unique<Impl>(total_vram_budget_bytes, policy)) {
    if (policy.use_gpu_probe) {
        impl_->probe = [] { return query_gpu_memory(); };
    }
    if (policy.pressure_poll_ms > 0) {
        Impl* impl = impl_.get();
        impl_->watchdog = std::thread([impl] {
            const auto slice = std::chrono::milliseconds(50);
            auto elapsed = std::chrono::milliseconds(0);
            const auto period = std::chrono::milliseconds(impl->policy.pressure_poll_ms);
            while (!impl->stop_watchdog.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(slice);
                elapsed += slice;
                if (elapsed < period) continue;
                elapsed = std::chrono::milliseconds(0);
                impl->watchdog_tick();
            }
        });
    }
}

VRAMResourceManager::~VRAMResourceManager() {
    impl_->stop_watchdog.store(true, std::memory_order_release);
    if (impl_->watchdog.joinable()) impl_->watchdog.join();
}

void VRAMResourceManager::register_resident(ResidentPtr resident) {
    if (!resident) return;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    const std::string id(resident->id());

    // Residents derivados de ResidentBase reportam mudanças de status
    // direto para o callback de telemetria do manager.
    if (auto* base = dynamic_cast<ResidentBase*>(resident.get())) {
        Impl* impl = impl_.get();
        base->set_status_listener(
            [impl](std::string_view rid, ResidentStatus o, ResidentStatus n) {
                ResidentStatusCallback cb;
                {
                    std::lock_guard<std::mutex> lk2(impl->status_cb_mtx);
                    cb = impl->on_status;
                }
                if (cb) cb(rid, o, n);
            });
    }
    impl_->residents[id] = std::move(resident);
    std::cout << "[vram] resident registrado: '" << id << "'" << std::endl;
}

void VRAMResourceManager::unregister_resident(std::string_view id) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->residents.erase(std::string(id));
    impl_->prefetches.erase(std::string(id));
    impl_->hot_evicted.erase(std::string(id));
}

std::future<bool> VRAMResourceManager::request_state(AlyssaState next) {
    std::vector<std::future<bool>> needed_loads;
    // Fase 5: job adiado — eviction primeiro, load da demanda depois.
    std::function<bool()> deferred_load;
    std::vector<ResidentPtr> reload_hots;
    AlyssaState from;
    StateTransitionCallback state_cb;
    std::function<void(bool)> vision_hook;
    bool vision_pause = false;

    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        from = impl_->state;
        impl_->state = next;
        state_cb = impl_->on_state;
        vision_hook = impl_->vision_action_locked(next, vision_pause);

        const char* wanted = jit_id_for(next);
        for (auto& [id, r] : impl_->residents) {
            if (r->tier() != ResidencyTier::TIER2_JIT) continue; // TIER0/1: intocáveis

            if (wanted && id == wanted) {
                impl_->prefetches.erase(id); // prefetch virou demanda formal
                const std::size_t need = r->metrics().vram_bytes_footprint;
                if (r->status() != ResidentStatus::RESIDENT &&
                    !impl_->fits_locked(need)) {
                    // Fase 5: eviction em cascata com o load adiado para
                    // depois das vítimas saírem (senão o backend disputa a
                    // VRAM com quem ainda está descarregando).
                    const std::size_t deficit = impl_->deficit_locked(need);
                    auto victims = impl_->pick_victims_locked(deficit, id);
                    ResidentPtr target = r;
                    deferred_load = [victims = std::move(victims), target]() mutable {
                        std::vector<std::future<bool>> evictions;
                        evictions.reserve(victims.size());
                        for (auto& v : victims) {
                            std::cout << "[vram] eviction: descarregando '"
                                      << v->id() << "' para abrir espaço" << std::endl;
                            evictions.push_back(v->unload_async());
                        }
                        for (auto& f : evictions) f.get(); // unload falho ≠ fatal
                        return target->load_async().get();
                    };
                } else {
                    needed_loads.push_back(r->load_async());
                }
            } else {
                // JIT fora de uso descarrega assim que possível (futuro
                // promise-based: descartar não bloqueia).
                auto f = r->unload_async();
                (void)f;
            }
        }

        // Fase 5: garantia de reload — HOTs evitados voltam quando o estado
        // não exige mais o JIT que causou a pressão e a folga real permite.
        if (!wanted && !impl_->hot_evicted.empty()) {
            for (auto it = impl_->hot_evicted.begin(); it != impl_->hot_evicted.end();) {
                auto rit = impl_->residents.find(*it);
                if (rit != impl_->residents.end() &&
                    impl_->fits_locked(rit->second->metrics().vram_bytes_footprint)) {
                    reload_hots.push_back(rit->second);
                    it = impl_->hot_evicted.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    if (state_cb) state_cb(from, next);
    if (vision_hook) vision_hook(vision_pause);
    for (auto& r : reload_hots) {
        std::cout << "[vram] reload pós-eviction: '" << r->id() << "'" << std::endl;
        auto f = r->load_async();
        (void)f; // fire-and-forget: reload não atrasa a transição
    }

    if (needed_loads.empty() && !deferred_load) {
        std::promise<bool> p;
        p.set_value(true);
        return p.get_future();
    }

    // Agrega os loads pendentes num único future (promise-based: o chamador
    // pode descartar sem bloquear).
    auto prom = std::make_shared<std::promise<bool>>();
    auto fut = prom->get_future();
    auto pending = std::make_shared<std::vector<std::future<bool>>>(std::move(needed_loads));
    std::thread([prom, pending, job = std::move(deferred_load)] {
        bool all_ok = true;
        for (auto& f : *pending) all_ok = f.get() && all_ok;
        if (job) all_ok = job() && all_ok;
        prom->set_value(all_ok);
    }).detach();
    return fut;
}

AlyssaState VRAMResourceManager::current_state() const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->state;
}

void VRAMResourceManager::prefetch(std::string_view resident_id) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    const std::string id(resident_id);
    auto it = impl_->residents.find(id);
    if (it == impl_->residents.end()) {
        std::cerr << "[vram] prefetch('" << id << "'): resident desconhecido" << std::endl;
        return;
    }
    ResidentPtr& r = it->second;
    if (r->tier() != ResidencyTier::TIER2_JIT) return; // TIER0/1 já são residentes
    if (r->status() == ResidentStatus::RESIDENT ||
        impl_->prefetches.count(id)) {
        return; // já carregado ou já em prefetch
    }
    // Dica de otimização, não demanda formal: sob pressão (contábil OU real,
    // Fase 5), descarta — prefetch nunca justifica eviction.
    if (impl_->pressured ||
        !impl_->fits_locked(r->metrics().vram_bytes_footprint)) {
        std::cout << "[vram] prefetch('" << id << "') descartado sob pressão de VRAM" << std::endl;
        return;
    }
    std::cout << "[vram] prefetch('" << id << "') disparado" << std::endl;
    impl_->prefetches[id] = r->load_async().share();
}

void VRAMResourceManager::cancel_prefetch(std::string_view resident_id) {
    std::shared_future<bool> inflight;
    ResidentPtr resident;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        const std::string id(resident_id);
        auto pit = impl_->prefetches.find(id);
        if (pit == impl_->prefetches.end()) return;
        inflight = pit->second;
        impl_->prefetches.erase(pit);

        // Se o estado atual passou a exigir o resident, o cancel perdeu a corrida.
        const char* wanted = jit_id_for(impl_->state);
        if (wanted && id == wanted) return;

        auto rit = impl_->residents.find(id);
        if (rit != impl_->residents.end()) resident = rit->second;
    }
    if (!resident) return;

    // Best effort: não dá para abortar a transferência no meio (whisper/llama
    // não expõem isso) — espera o load resolver e descarrega em seguida.
    std::thread([inflight, resident] {
        if (inflight.valid()) inflight.wait();
        auto f = resident->unload_async();
        (void)f;
    }).detach();
}

std::size_t VRAMResourceManager::total_vram_budget_bytes() const noexcept {
    return impl_->budget;
}

std::size_t VRAMResourceManager::used_vram_bytes() const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->used_locked();
}

std::size_t VRAMResourceManager::free_vram_bytes() const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    const std::size_t used = impl_->used_locked();
    return used >= impl_->budget ? 0 : impl_->budget - used;
}

std::optional<std::size_t> VRAMResourceManager::real_free_vram_bytes() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (const auto mem = impl_->probe_locked()) return mem->free_bytes;
    return std::nullopt;
}

void VRAMResourceManager::set_gpu_memory_probe(
    std::function<std::optional<GpuMemInfo>()> probe) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->probe = std::move(probe);
    impl_->probe_cache.reset(); // fonte trocou: cache antigo não vale
}

void VRAMResourceManager::set_pressure_callback(PressureCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->on_pressure = std::move(cb);
}

std::optional<ResidentStatus>
VRAMResourceManager::status_of(std::string_view resident_id) const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->residents.find(std::string(resident_id));
    if (it == impl_->residents.end()) return std::nullopt;
    return it->second->status();
}

std::vector<std::string> VRAMResourceManager::resident_ids() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::vector<std::string> ids;
    ids.reserve(impl_->residents.size());
    for (const auto& [id, r] : impl_->residents) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void VRAMResourceManager::set_state_transition_callback(StateTransitionCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->on_state = std::move(cb);
}

void VRAMResourceManager::set_resident_status_callback(ResidentStatusCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->status_cb_mtx);
    impl_->on_status = std::move(cb);
}

void VRAMResourceManager::set_vision_throttle_enabled(bool enabled) {
    std::function<void(bool)> hook;
    bool pause = false;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->vision_throttle = enabled;
        if (!enabled && impl_->vision_paused && impl_->vision_hook) {
            // Desligar o throttle nunca deixa vision pausado para trás.
            impl_->vision_paused = false;
            hook = impl_->vision_hook;
            pause = false;
        } else if (enabled) {
            hook = impl_->vision_action_locked(impl_->state, pause);
        }
    }
    if (hook) hook(pause);
}

bool VRAMResourceManager::vision_throttle_enabled() const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->vision_throttle;
}

void VRAMResourceManager::set_vision_pause_hook(std::function<void(bool)> hook) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->vision_hook = std::move(hook);
}

} // namespace alyssa::vram
