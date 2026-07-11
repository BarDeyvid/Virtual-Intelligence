#include "GpuMemoryProbe.hpp"

#include <cstdio>
#include <cstdlib>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <dlfcn.h>
#define POPEN popen
#define PCLOSE pclose
#endif

namespace alyssa::vram {

namespace {

// --- NVML dinâmica (assinaturas estáveis desde os primeiros drivers) --------
using nvmlReturn_t = int; // 0 = NVML_SUCCESS
using nvmlDevice_t = void*;
struct nvmlMemory_t {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};
using nvmlInit_t         = nvmlReturn_t (*)();
using nvmlGetHandle_t    = nvmlReturn_t (*)(unsigned int, nvmlDevice_t*);
using nvmlGetMemInfo_t   = nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*);

struct Nvml {
    nvmlGetMemInfo_t get_mem = nullptr;
    nvmlDevice_t device = nullptr;
    bool ok = false;

    Nvml() {
#ifdef _WIN32
        HMODULE h = LoadLibraryA("nvml.dll"); // instalada pelo driver, no System32
        if (!h) return;
        auto init = reinterpret_cast<nvmlInit_t>(GetProcAddress(h, "nvmlInit_v2"));
        auto handle = reinterpret_cast<nvmlGetHandle_t>(
            GetProcAddress(h, "nvmlDeviceGetHandleByIndex_v2"));
        get_mem = reinterpret_cast<nvmlGetMemInfo_t>(
            GetProcAddress(h, "nvmlDeviceGetMemoryInfo"));
#else
        void* h = dlopen("libnvidia-ml.so.1", RTLD_NOW);
        if (!h) h = dlopen("libnvidia-ml.so", RTLD_NOW);
        if (!h) return;
        auto init = reinterpret_cast<nvmlInit_t>(dlsym(h, "nvmlInit_v2"));
        auto handle = reinterpret_cast<nvmlGetHandle_t>(
            dlsym(h, "nvmlDeviceGetHandleByIndex_v2"));
        get_mem = reinterpret_cast<nvmlGetMemInfo_t>(
            dlsym(h, "nvmlDeviceGetMemoryInfo"));
#endif
        if (!init || !handle || !get_mem) return;
        if (init() != 0) return;
        if (handle(0, &device) != 0) return;
        ok = true;
    }
};

// Fallback lento: parse do nvidia-smi (usado só quando a NVML não abre).
std::optional<GpuMemInfo> query_via_smi() {
    FILE* pipe = POPEN(
        "nvidia-smi --query-gpu=memory.total,memory.free,memory.used "
        "--format=csv,noheader,nounits", "r");
    if (!pipe) return std::nullopt;
    char buf[128] = {0};
    const bool got = fgets(buf, sizeof(buf), pipe) != nullptr;
    PCLOSE(pipe);
    if (!got) return std::nullopt;

    char* end = nullptr;
    const long total = std::strtol(buf, &end, 10);
    while (end && (*end == ',' || *end == ' ')) ++end;
    const long free_ = std::strtol(end, &end, 10);
    while (end && (*end == ',' || *end == ' ')) ++end;
    const long used = std::strtol(end, nullptr, 10);
    if (total <= 0) return std::nullopt;

    constexpr std::size_t MiB = 1024ull * 1024;
    return GpuMemInfo{static_cast<std::size_t>(total) * MiB,
                      static_cast<std::size_t>(free_) * MiB,
                      static_cast<std::size_t>(used) * MiB};
}

} // namespace

std::optional<GpuMemInfo> query_gpu_memory() {
    static Nvml nvml;                 // init única, thread-safe (magic static)
    static std::mutex nvml_call_mtx;  // chamadas NVML serializadas por via das dúvidas

    if (nvml.ok) {
        nvmlMemory_t mem{};
        std::lock_guard<std::mutex> lk(nvml_call_mtx);
        if (nvml.get_mem(nvml.device, &mem) == 0) {
            return GpuMemInfo{static_cast<std::size_t>(mem.total),
                              static_cast<std::size_t>(mem.free),
                              static_cast<std::size_t>(mem.used)};
        }
    }
    return query_via_smi();
}

} // namespace alyssa::vram
