// GameplayLog.cpp
#include "minecraft/GameplayLog.hpp"

#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace alyssa_minecraft {

namespace {

std::string timestamp_now() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    std::ostringstream out;
    out << std::put_time(&local_tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return out.str();
}

} // namespace

GameplayLog::GameplayLog() {
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    file.open("logs/minecraft_session.jsonl", std::ios::app);
}

GameplayLog& GameplayLog::instance() {
    static GameplayLog log;
    return log;
}

void GameplayLog::log(const std::string& event, const nlohmann::json& fields) {
    nlohmann::json entry = fields;
    entry["ts"] = timestamp_now();
    entry["event"] = event;

    std::lock_guard<std::mutex> lock(mtx);
    if (!file.is_open()) return;
    file << entry.dump() << '\n';
    file.flush(); // crash-safety over throughput — this is a low-frequency (per-tick) log
}

} // namespace alyssa_minecraft
