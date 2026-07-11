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

private:
    GameplayLog();

    std::mutex mtx;
    std::ofstream file;
};

} // namespace alyssa_minecraft
