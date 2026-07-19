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

GameplayLog::~GameplayLog() {
    std::lock_guard<std::mutex> lock(mtx);
    flush_repeat_marker_locked();
}

void GameplayLog::set_tick(long long tick_id) {
    std::lock_guard<std::mutex> lock(mtx);
    current_tick = tick_id;
}

void GameplayLog::flush_repeat_marker_locked() {
    if (repeat_count <= 0) return;
    nlohmann::json marker = last_fields;
    marker["event"] = last_event;
    marker["ts"] = last_written_ts;   // ts/tick of the LAST occurrence, so the
    if (last_written_tick >= 0) {     // marker closes the span the run covered
        marker["tick"] = last_written_tick;
    }
    marker["repeated"] = repeat_count; // additional occurrences beyond the one already written
    if (file.is_open()) {
        file << marker.dump() << '\n';
        file.flush();
    }
    repeat_count = 0;
}

void GameplayLog::log(const std::string& event, const nlohmann::json& fields) {
    std::string ts = timestamp_now();

    std::lock_guard<std::mutex> lock(mtx);
    if (!file.is_open()) return;

    if (has_last && event == last_event && fields == last_fields) {
        // Byte-identical to the last entry apart from ts — don't write a
        // new line, just extend the run. Flushed once something different
        // finally comes in (or at shutdown).
        ++repeat_count;
        last_written_ts = ts;
        last_written_tick = current_tick;
        return;
    }

    flush_repeat_marker_locked(); // close out the previous entry's run, if any

    nlohmann::json entry = fields;
    entry["ts"] = ts;
    entry["event"] = event;
    if (current_tick >= 0) entry["tick"] = current_tick;
    file << entry.dump() << '\n';
    file.flush(); // crash-safety over throughput — this is a low-frequency (per-tick) log

    last_event = event;
    last_fields = fields;
    last_written_ts = ts;
    last_written_tick = current_tick;
    has_last = true;
}

} // namespace alyssa_minecraft
