// MinecraftBridge.hpp
#pragma once

#include <string>
#include <deque>
#include <mutex>
#include "json.hpp"

namespace alyssa_minecraft {

using json = nlohmann::json;

/**
 * @class MinecraftBridge
 * @brief TCP/JSONL client for the Node.js mineflayer sidecar (minecraft-bridge/).
 *
 * The sidecar owns the actual Minecraft connection; this class only talks the
 * local newline-delimited-JSON protocol defined in minecraft-bridge/index.js:
 *   -> {"type":"get_state"}                          <- {"type":"state","data":{...}}
 *   -> {"type":"action","verb":"...","args":[...]}    <- {"type":"action_result","ok":bool,"message":"..."}
 *   <- {"type":"event","event":"damage|death|chat","data":{...}}  (unsolicited, queued)
 *
 * Not thread-safe for concurrent request calls (mirrors CoreIntegration's
 * brain_mtx convention — callers serialize access).
 */
class MinecraftBridge {
public:
    MinecraftBridge() = default;
    ~MinecraftBridge();

    MinecraftBridge(const MinecraftBridge&) = delete;
    MinecraftBridge& operator=(const MinecraftBridge&) = delete;

    /// Connects to the sidecar. host/port refer to the LOCAL bridge socket
    /// (minecraft-bridge's BRIDGE_PORT), not the Minecraft server itself.
    bool connect(const std::string& host = "127.0.0.1", int port = 8765, int timeout_ms = 5000);
    void disconnect();
    bool is_connected() const { return connected; }

    /// Blocking request/response. Returns an empty json object on I/O failure.
    json get_world_state();

    /// verb: "mover" | "minerar" | "colocar" | "atacar" | "craftar" | "falar" | "esperar"
    json send_action(const std::string& verb, const std::vector<std::string>& args);

    /// Drains and returns any unsolicited sidecar events (damage/death/chat)
    /// accumulated since the last call. Non-blocking.
    std::vector<json> drain_events();

private:
    bool connected = false;
#ifdef _WIN32
    uintptr_t sock = 0; // SOCKET, avoids pulling <winsock2.h> into this header
#else
    int sock = -1;
#endif
    std::string recv_buffer;
    std::deque<json> pending_events;

    bool send_line(const json& msg);
    /// Reads lines until a non-"event" message arrives (queuing events as they pass).
    /// Returns an empty object if the connection drops or times out.
    json read_reply();
    bool read_line(std::string& out_line);
};

} // namespace alyssa_minecraft
