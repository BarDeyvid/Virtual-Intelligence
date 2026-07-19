// GameplayLog.hpp
#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include "json.hpp"

namespace alyssa_minecraft {

/**
 * @class GameplayLog
 * @brief Append-only JSONL log for the Minecraft subsystem.
 *
 * The FTXUI Logs tab interleaves every subsystem's std::cout/cerr on one
 * scrolling pane with no way to copy it out. This writes one self-contained
 * JSON object per line to logs/minecraft_session.jsonl instead — every tick's
 * world state, prompt, raw signal, parsed action, execution result, and any
 * drained sidecar events, each independently greppable/parseable.
 *
 * Thread-safe (MinecraftSession's tick thread and any future caller can log
 * concurrently); logs/ already exists project-wide and is gitignored.
 */
class GameplayLog {
public:
    static GameplayLog& instance();

    /// fields is merged into {"ts": ..., "event": event, ...fields}.
    void log(const std::string& event, const nlohmann::json& fields = nlohmann::json::object());

    /// Stamps every subsequent entry with {"tick": tick_id} so all the lines
    /// of one gameplay tick (tick_start, world_state, prompt, signal,
    /// action_*) are correlatable by the analysis tooling
    /// (tools/analyze_gameplay.js) without guessing by timestamp proximity.
    /// MinecraftSession::tick_loop calls this once per tick.
    void set_tick(long long tick_id);

    ~GameplayLog();

private:
    GameplayLog();

    std::mutex mtx;
    std::ofstream file;

    // Collapses runs of consecutive entries that are identical apart from
    // ts (e.g. a stuck bridge writing an empty world_state every 3s, or the
    // "moving toward (x,y,z)" spam while she oscillates between two nearly
    // identical labels) into one trailing {"repeated": N} marker instead of
    // one full line per occurrence — same idea as syslog's "last message
    // repeated N times", just applied per-event here.
    std::string last_event;
    nlohmann::json last_fields;
    std::string last_written_ts;
    long long repeat_count = 0;
    bool has_last = false;

    // Stamped alongside ts (i.e. NOT part of the dedup comparison — a run of
    // identical idle ticks still collapses even though each has its own id).
    long long current_tick = -1;
    long long last_written_tick = -1;

    /// Caller must hold mtx. Writes the pending "repeated N times" marker
    /// for the last entry, if any repeats accumulated, then resets it.
    void flush_repeat_marker_locked();
};

} // namespace alyssa_minecraft
