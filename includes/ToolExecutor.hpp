/**
 * @file ToolExecutor.hpp
 * @brief Registry-driven tool system for AlyssaNet (TODO Phase 1.1 / 1.2).
 *
 * Loads tool specs from config/tools_registry.json, injects a compact
 * description block into LLM prompts, parses [TOOL_CALL] blocks from LLM
 * output and dispatches them to registered C++ handlers with timeout and
 * parameter validation.
 *
 * The executor itself knows nothing about the concrete tools: handlers are
 * registered at runtime (see register_default_handlers() for the generic
 * filesystem/time ones; CoreIntegration registers the ones that need heavy
 * dependencies like OpenCV/CURL).
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

namespace alyssa_tools {

/**
 * @struct ToolParam
 * @brief Declared parameter of a tool in the registry.
 */
struct ToolParam {
    std::string name;
    std::string type = "string";     ///< "string" | "int" | "float" | "bool" (informational)
    bool required = false;
    std::string default_value;       ///< Used when the LLM omits the param
    bool has_default = false;
};

/**
 * @struct ToolSpec
 * @brief One tool entry from tools_registry.json.
 */
struct ToolSpec {
    std::string name;
    std::string description;
    std::vector<ToolParam> params;
    int timeout_s = 0;               ///< 0 = use registry default_timeout_s
};

/**
 * @struct ToolCall
 * @brief A parsed [TOOL_CALL] name(param=value) [/TOOL_CALL] block.
 */
struct ToolCall {
    std::string name;
    std::map<std::string, std::string> args;
};

/**
 * @struct ToolResult
 * @brief Outcome of executing one ToolCall.
 */
struct ToolResult {
    bool success = false;
    std::string output;              ///< Tool output on success, error message otherwise
};

/**
 * @struct ToolCallRecord
 * @brief Log entry for debugging / future UI tab (TODO Phase 5.2).
 */
struct ToolCallRecord {
    std::string tool_name;
    std::map<std::string, std::string> args;
    bool success = false;
    std::string output_preview;      ///< First 200 chars of the result
    long long duration_ms = 0;
    std::string timestamp;           ///< "YYYY-MM-DD HH:MM:SS" local time
};

/// Handler signature: receives validated args (defaults filled), returns output.
/// Throwing std::exception inside a handler produces a failed ToolResult.
using ToolHandler = std::function<std::string(const std::map<std::string, std::string>&)>;

/**
 * @class ToolExecutor
 * @brief Loads the tool registry and executes parsed tool calls.
 *
 * Thread-safety: execute() and the call log are mutex-protected; loading and
 * handler registration are expected to happen once during initialization.
 */
class ToolExecutor {
public:
    /**
     * @brief Load tool specs from a JSON registry file.
     * @return true on success; false if the file is missing/malformed (the
     *         executor stays usable with zero tools — graceful degradation).
     */
    bool load_registry(const std::string& registry_path);

    /**
     * @brief Register the C++ implementation of a tool.
     * @details Handlers for names absent from the registry are kept but never
     *          exposed in the prompt nor callable by the LLM.
     */
    void register_handler(const std::string& tool_name, ToolHandler handler);

    /**
     * @brief Register the built-in dependency-free handlers
     *        (get_datetime, read_file, list_dir).
     */
    void register_default_handlers();

    /**
     * @brief Compact prompt block describing the available tools.
     * @return Empty string when no registered tool has a handler, otherwise a
     *         [FERRAMENTAS] block with one line per usable tool.
     */
    std::string get_tools_prompt() const;

    /**
     * @brief Parse all [TOOL_CALL] name(a=1, b="x") [/TOOL_CALL] blocks.
     * @details Malformed blocks are skipped (logged to stderr), never fatal.
     */
    static std::vector<ToolCall> parse_tool_calls(const std::string& llm_output);

    /**
     * @brief Remove every [TOOL_CALL]...[/TOOL_CALL] block from a response.
     */
    static std::string strip_tool_calls(const std::string& text);

    /**
     * @brief Validate and run a tool call with timeout.
     * @details Unknown tool, missing required param or handler exception all
     *          return a failed ToolResult whose output explains the problem —
     *          the message is fed back to the LLM so it can react.
     *          A handler that exceeds its timeout is abandoned (detached
     *          thread keeps running — XGH, documented limitation).
     */
    ToolResult execute(const ToolCall& call);

    bool has_tool(const std::string& name) const;
    const std::vector<ToolSpec>& get_specs() const { return specs; }
    std::vector<ToolCallRecord> get_call_log() const;

    int max_rounds() const { return max_rounds_; }
    int max_calls_per_round() const { return max_calls_per_round_; }

private:
    const ToolSpec* find_spec(const std::string& name) const;

    std::vector<ToolSpec> specs;
    std::map<std::string, ToolHandler> handlers;
    std::vector<ToolCallRecord> call_log;
    mutable std::mutex log_mtx;

    int max_rounds_ = 2;
    int max_calls_per_round_ = 3;
    int default_timeout_s_ = 10;
};

} // namespace alyssa_tools
