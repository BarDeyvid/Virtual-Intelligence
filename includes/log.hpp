#include <iostream>

#include "spdlog/spdlog.h"
#include <spdlog/sinks/stdout_color_sinks.h>    
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <memory>

class Log {
public:
    static std::shared_ptr<spdlog::logger>& getLogger() {
        // Check if the logger already exists
        static auto logger = spdlog::get("AI_Log");
        if (!logger) {
            logger = spdlog::stdout_color_mt("AI_Log");
        }
        return logger;
    }

    static void init(const std::string& log_file_path) {
        try {
            // Check if the logger already exists
            auto existing_logger = spdlog::get("AI_Log");
            if (existing_logger) {
                spdlog::drop_all();  // Drop all existing loggers
            }
            
            // Create a file sink with rotation policy
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/" + log_file_path, 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::debug);
            
            // Create a console sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            
            // Set the pattern for both sinks
            auto formatter = std::make_unique<spdlog::pattern_formatter>("[%Y-%m-%d %H:%M:%S] [%^%l%^] [%n]: %v");
            file_sink->set_formatter(std::move(formatter));
            console_sink->set_formatter(std::move(formatter));

            // Create a logger with both sinks
            auto logger = std::shared_ptr<spdlog::logger>(new spdlog::logger("AI_Log", {file_sink, console_sink}));
            spdlog::register_logger(logger);
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }
};
