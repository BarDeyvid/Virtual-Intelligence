#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <mutex>

namespace logging {

enum class Level { DEBUG, INFO, WARN};

struct Logger {
    std::ofstream file;
    std::mutex mtx;

    explicit Logger(const std::string &filename = "server.log") {
        file.open(filename, std::ios::app);
    }

    void write(Level lvl, const std::string &msg) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string prefix;
        switch (lvl) { case Level::DEBUG: prefix = "[D] "; break;
                       case Level::INFO :  prefix = "[I] "; break;
                       case Level::WARN :  prefix = "[W] "; break; }
        std::string line = prefix + msg + "\n";
        std::cout << line;
        if (file.is_open()) file << line;
    }

    void debug(const std::string &msg) { write(Level::DEBUG, msg); }
    void info (const std::string &msg) { write(Level::INFO , msg); }
    void warn (const std::string &msg) { write(Level::WARN , msg); }
};

inline Logger &instance() {
    static Logger logger;
    return logger;
}

} // namespace log
