// test_tool_executor.cpp
// Unit tests for the ToolExecutor (registry loading, [TOOL_CALL] parsing,
// dispatch, validation, timeout). No model loading, no llama.cpp — fast.

#include "../includes/ToolExecutor.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace alyssa_tools;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr)                                                     \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "FAIL  " << name << "\n";                          \
            ++tests_failed;                                                  \
        } else {                                                             \
            ++tests_passed;                                                  \
        }                                                                    \
    } while(0)

static const char* TEST_REGISTRY_PATH = "test_tools_registry.json";

static void write_test_registry() {
    std::ofstream f(TEST_REGISTRY_PATH);
    f << R"({
        "settings": { "max_rounds": 2, "max_calls_per_round": 3, "default_timeout_s": 10 },
        "tools": [
            { "name": "echo", "description": "Repete o texto",
              "params": [ {"name": "text", "type": "string", "required": true} ] },
            { "name": "greet", "description": "Cumprimenta",
              "params": [ {"name": "who", "type": "string", "required": false, "default": "mundo"} ] },
            { "name": "slow", "description": "Demora de propósito",
              "params": [], "timeout_s": 1 },
            { "name": "ghost", "description": "Sem handler registrado", "params": [] }
        ]
    })";
}

// =============================================================================
// parse_tool_calls
// =============================================================================
static void test_parsing() {
    auto calls = ToolExecutor::parse_tool_calls(
        "vou verificar [TOOL_CALL] echo(text=oi) [/TOOL_CALL] agora");
    TEST("single call parsed",            calls.size() == 1);
    TEST("single call name",              !calls.empty() && calls[0].name == "echo");
    TEST("single call arg",               !calls.empty() && calls[0].args.at("text") == "oi");

    calls = ToolExecutor::parse_tool_calls(
        "[TOOL_CALL] echo(text=\"oi, tudo bem?\") [/TOOL_CALL]");
    TEST("quoted arg keeps comma",        !calls.empty() && calls[0].args.at("text") == "oi, tudo bem?");

    calls = ToolExecutor::parse_tool_calls(
        "[TOOL_CALL] read_file(path=config/a.json, max_lines=5) [/TOOL_CALL]");
    TEST("two args parsed",               !calls.empty() && calls[0].args.size() == 2);
    TEST("second arg value",              !calls.empty() && calls[0].args.at("max_lines") == "5");

    calls = ToolExecutor::parse_tool_calls(
        "a [TOOL_CALL] one() [/TOOL_CALL] b [TOOL_CALL] two(x=1) [/TOOL_CALL] c");
    TEST("multiple calls parsed",         calls.size() == 2);
    TEST("multiple calls order",          calls.size() == 2 && calls[0].name == "one" && calls[1].name == "two");

    calls = ToolExecutor::parse_tool_calls("[TOOL_CALL] get_datetime [/TOOL_CALL]");
    TEST("call without parens",           calls.size() == 1 && calls[0].name == "get_datetime");

    calls = ToolExecutor::parse_tool_calls("sem chamadas aqui");
    TEST("no calls in plain text",        calls.empty());

    calls = ToolExecutor::parse_tool_calls("[TOOL_CALL] quebrado(x=1"); // sem fechamento
    TEST("unclosed block ignored",        calls.empty());

    calls = ToolExecutor::parse_tool_calls("[TOOL_CALL] mal(formado [/TOOL_CALL]");
    TEST("malformed parens skipped",      calls.empty());

    calls = ToolExecutor::parse_tool_calls("[TOOL_CALL]  [/TOOL_CALL]");
    TEST("empty block skipped",           calls.empty());
}

// =============================================================================
// strip_tool_calls
// =============================================================================
static void test_strip() {
    TEST("strip removes block",
         ToolExecutor::strip_tool_calls("olá [TOOL_CALL] x() [/TOOL_CALL] mundo") == "olá  mundo");
    TEST("strip keeps clean text",
         ToolExecutor::strip_tool_calls("texto limpo") == "texto limpo");
    TEST("strip cuts unclosed block",
         ToolExecutor::strip_tool_calls("resposta [TOOL_CALL] x(") == "resposta");
    TEST("strip multiple blocks",
         ToolExecutor::strip_tool_calls(
             "[TOOL_CALL] a() [/TOOL_CALL]meio[TOOL_CALL] b() [/TOOL_CALL]") == "meio");
}

// =============================================================================
// registry + prompt
// =============================================================================
static void test_registry_and_prompt() {
    ToolExecutor exec;
    TEST("missing registry returns false", !exec.load_registry("nao_existe.json"));
    TEST("missing registry keeps prompt empty", exec.get_tools_prompt().empty());

    write_test_registry();
    ToolExecutor exec2;
    TEST("registry loads",                exec2.load_registry(TEST_REGISTRY_PATH));
    TEST("registry spec count",           exec2.get_specs().size() == 4);
    TEST("settings loaded",               exec2.max_rounds() == 2 && exec2.max_calls_per_round() == 3);

    // Sem handler registrado, nada é exposto no prompt
    TEST("prompt empty without handlers", exec2.get_tools_prompt().empty());

    exec2.register_handler("echo", [](const std::map<std::string, std::string>& args) {
        return args.at("text");
    });
    std::string prompt = exec2.get_tools_prompt();
    TEST("prompt lists echo",             prompt.find("echo(text)") != std::string::npos);
    TEST("prompt has instructions",       prompt.find("[TOOL_CALL]") != std::string::npos);
    TEST("prompt hides handler-less tool", prompt.find("ghost") == std::string::npos);

    exec2.register_handler("greet", [](const std::map<std::string, std::string>& args) {
        return "olá " + args.at("who");
    });
    prompt = exec2.get_tools_prompt();
    TEST("optional param marked with ?",  prompt.find("greet(who?)") != std::string::npos);
}

// =============================================================================
// execute: dispatch, validation, defaults, timeout
// =============================================================================
static void test_execute() {
    write_test_registry();
    ToolExecutor exec;
    exec.load_registry(TEST_REGISTRY_PATH);

    exec.register_handler("echo", [](const std::map<std::string, std::string>& args) {
        return args.at("text");
    });
    exec.register_handler("greet", [](const std::map<std::string, std::string>& args) {
        return "olá " + args.at("who");
    });
    exec.register_handler("slow", [](const std::map<std::string, std::string>&) -> std::string {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return "nunca chega";
    });

    ToolCall echo_call{"echo", {{"text", "funciona"}}};
    ToolResult r = exec.execute(echo_call);
    TEST("echo succeeds",                 r.success && r.output == "funciona");

    ToolCall unknown{"inexistente", {}};
    r = exec.execute(unknown);
    TEST("unknown tool fails",            !r.success);
    TEST("unknown tool message",          r.output.find("desconhecida") != std::string::npos);

    ToolCall ghost{"ghost", {}};
    r = exec.execute(ghost);
    TEST("handler-less tool fails",       !r.success);

    ToolCall missing_required{"echo", {}};
    r = exec.execute(missing_required);
    TEST("missing required param fails",  !r.success);
    TEST("missing param names it",        r.output.find("text") != std::string::npos);

    ToolCall default_fill{"greet", {}};
    r = exec.execute(default_fill);
    TEST("default value filled",          r.success && r.output == "olá mundo");

    ToolCall explicit_arg{"greet", {{"who", "Deyvid"}}};
    r = exec.execute(explicit_arg);
    TEST("explicit overrides default",    r.success && r.output == "olá Deyvid");

    ToolCall slow_call{"slow", {}};
    auto start = std::chrono::steady_clock::now();
    r = exec.execute(slow_call);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    TEST("slow tool times out",           !r.success && r.output.find("timeout") != std::string::npos);
    TEST("timeout respects per-tool 1s",  elapsed < 2500);

    exec.register_handler("boom", [](const std::map<std::string, std::string>&) -> std::string {
        throw std::runtime_error("explodiu");
    });
    // "boom" não está no registro → desconhecida mesmo com handler
    ToolCall boom{"boom", {}};
    r = exec.execute(boom);
    TEST("handler outside registry blocked", !r.success);

    TEST("call log recorded",             exec.get_call_log().size() >= 7);
    TEST("call log has tool name",        exec.get_call_log()[0].tool_name == "echo");
}

// =============================================================================
// default handlers (read_file / list_dir / get_datetime)
// =============================================================================
static void test_default_handlers() {
    write_test_registry();

    // Registro próprio cobrindo os handlers default
    std::ofstream f("test_default_registry.json");
    f << R"({
        "tools": [
            { "name": "get_datetime", "description": "d", "params": [] },
            { "name": "read_file", "description": "r",
              "params": [ {"name": "path", "type": "string", "required": true},
                          {"name": "max_lines", "type": "int", "required": false, "default": "100"} ] },
            { "name": "list_dir", "description": "l",
              "params": [ {"name": "path", "type": "string", "required": false, "default": "."} ] }
        ]
    })";
    f.close();

    ToolExecutor exec;
    exec.load_registry("test_default_registry.json");
    exec.register_default_handlers();

    ToolResult r = exec.execute({"get_datetime", {}});
    TEST("get_datetime works",            r.success && r.output.size() >= 19);

    std::filesystem::create_directory("test_tool_dir");
    std::ofstream sample("test_tool_dir/test_sample.txt");
    sample << "linha1\nlinha2\nlinha3\n";
    sample.close();

    r = exec.execute({"read_file", {{"path", "test_tool_dir/test_sample.txt"}}});
    TEST("read_file reads content",       r.success && r.output.find("linha2") != std::string::npos);

    r = exec.execute({"read_file", {{"path", "test_tool_dir/test_sample.txt"}, {"max_lines", "1"}}});
    TEST("read_file respects max_lines",  r.success && r.output.find("linha2") == std::string::npos);

    r = exec.execute({"read_file", {{"path", "../fora.txt"}}});
    TEST("read_file blocks traversal",    !r.success);

    r = exec.execute({"read_file", {{"path", "C:/Windows/win.ini"}}});
    TEST("read_file blocks system paths", !r.success);

    r = exec.execute({"list_dir", {{"path", "~"}}});
    TEST("list_dir expands tilde to home", r.success);

    r = exec.execute({"list_dir", {{"path", "test_tool_dir"}}});
    TEST("list_dir lists file",           r.success && r.output.find("test_sample.txt") != std::string::npos);

    r = exec.execute({"list_dir", {{"path", "dir_que_nao_existe"}}});
    TEST("list_dir missing dir fails",    !r.success);

    std::filesystem::remove_all("test_tool_dir");
    std::remove("test_default_registry.json");
}

int main() {
    test_parsing();
    test_strip();
    test_registry_and_prompt();
    test_execute();
    test_default_handlers();

    std::remove(TEST_REGISTRY_PATH);

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << tests_passed << "  Failed: " << tests_failed << "\n";
    std::cout << "========================================\n";
    return tests_failed == 0 ? 0 : 1;
}
