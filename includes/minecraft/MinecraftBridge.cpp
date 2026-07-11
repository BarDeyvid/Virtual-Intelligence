// MinecraftBridge.cpp
#include "minecraft/MinecraftBridge.hpp"

#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    using socket_t = int;
    static constexpr socket_t INVALID_SOCK = -1;
#endif

namespace alyssa_minecraft {

#ifdef _WIN32
static int wsa_ref_count = 0;

static bool wsa_acquire() {
    if (wsa_ref_count == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
    }
    wsa_ref_count++;
    return true;
}

static void wsa_release() {
    if (--wsa_ref_count <= 0) {
        WSACleanup();
        wsa_ref_count = 0;
    }
}
#endif

static void close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

MinecraftBridge::~MinecraftBridge() {
    disconnect();
}

bool MinecraftBridge::connect(const std::string& host, int port, int timeout_ms) {
    if (connected) {
        disconnect();
    }

#ifdef _WIN32
    if (!wsa_acquire()) {
        std::cerr << "[MinecraftBridge] WSAStartup falhou." << std::endl;
        return false;
    }
#endif

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0 || !result) {
        std::cerr << "[MinecraftBridge] Falha ao resolver " << host << ":" << port << std::endl;
#ifdef _WIN32
        wsa_release();
#endif
        return false;
    }

    socket_t s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCK) {
        std::cerr << "[MinecraftBridge] Falha ao criar socket." << std::endl;
        freeaddrinfo(result);
#ifdef _WIN32
        wsa_release();
#endif
        return false;
    }

#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    if (::connect(s, result->ai_addr, static_cast<int>(result->ai_addrlen)) != 0) {
        std::cerr << "[MinecraftBridge] Falha ao conectar em " << host << ":" << port
                  << " (o sidecar minecraft-bridge está rodando?)" << std::endl;
        freeaddrinfo(result);
        close_socket(s);
#ifdef _WIN32
        wsa_release();
#endif
        return false;
    }

    freeaddrinfo(result);
    sock = static_cast<decltype(sock)>(s);
    connected = true;
    recv_buffer.clear();
    return true;
}

void MinecraftBridge::disconnect() {
    if (!connected) return;
    close_socket(static_cast<socket_t>(sock));
    connected = false;
#ifdef _WIN32
    wsa_release();
#endif
}

bool MinecraftBridge::send_line(const json& msg) {
    if (!connected) return false;
    std::string line = msg.dump() + "\n";
    size_t total_sent = 0;
    while (total_sent < line.size()) {
        int n = send(static_cast<socket_t>(sock), line.data() + total_sent,
                     static_cast<int>(line.size() - total_sent), 0);
        if (n <= 0) {
            std::cerr << "[MinecraftBridge] Falha ao enviar; conexão perdida." << std::endl;
            connected = false;
            return false;
        }
        total_sent += static_cast<size_t>(n);
    }
    return true;
}

bool MinecraftBridge::read_line(std::string& out_line) {
    size_t newline_pos;
    while ((newline_pos = recv_buffer.find('\n')) == std::string::npos) {
        if (!connected) return false;
        char buf[4096];
        int n = recv(static_cast<socket_t>(sock), buf, sizeof(buf), 0);
        if (n <= 0) {
            connected = false;
            return false;
        }
        recv_buffer.append(buf, n);
    }
    out_line = recv_buffer.substr(0, newline_pos);
    recv_buffer.erase(0, newline_pos + 1);
    return true;
}

json MinecraftBridge::read_reply() {
    while (true) {
        std::string line;
        if (!read_line(line)) {
            return json::object();
        }
        if (line.empty()) continue;

        json msg;
        try {
            msg = json::parse(line);
        } catch (const std::exception& e) {
            std::cerr << "[MinecraftBridge] Linha inválida do sidecar: " << e.what() << std::endl;
            continue;
        }

        if (msg.value("type", "") == "event") {
            pending_events.push_back(msg);
            continue;
        }
        return msg;
    }
}

json MinecraftBridge::get_world_state() {
    if (!send_line(json{{"type", "get_state"}})) {
        return json::object();
    }
    json reply = read_reply();
    if (reply.value("type", "") == "state" && reply.contains("data")) {
        return reply["data"];
    }
    return json::object();
}

json MinecraftBridge::send_action(const std::string& verb, const std::vector<std::string>& args) {
    json request = {{"type", "action"}, {"verb", verb}, {"args", args}};
    if (!send_line(request)) {
        return json{{"ok", false}, {"message", "bridge disconnected"}};
    }
    json reply = read_reply();
    if (reply.value("type", "") == "action_result") {
        return reply;
    }
    return json{{"ok", false}, {"message", "no response from sidecar"}};
}

std::vector<json> MinecraftBridge::drain_events() {
    std::vector<json> events(pending_events.begin(), pending_events.end());
    pending_events.clear();
    return events;
}

} // namespace alyssa_minecraft
