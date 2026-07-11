// TerminalSafety.cpp
#include "TerminalSafety.hpp"

#include <cstdlib>
#include <exception>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <csignal>
#endif

namespace alyssa_safety {

namespace {

// Show cursor, disable mouse-tracking modes (no-op if never enabled), exit
// the alternate screen buffer, reset SGR attributes. Mirrors what
// ScreenInteractive::Uninstall() writes on a clean shutdown.
constexpr char kResetSequence[] =
    "\x1b[?25h"
    "\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1006l"
    "\x1b[?1049l"
    "\x1b[0m\r\n";
constexpr int kResetSequenceLen = sizeof(kResetSequence) - 1;

void previous_terminate_and_abort() {
    write_terminal_reset();
    std::abort();
}

#ifdef _WIN32

BOOL WINAPI console_ctrl_handler(DWORD /*ctrl_type*/) {
    write_terminal_reset();
    // Let any other registered handler (including FTXUI's own) still run —
    // we only guarantee the reset happened first.
    return FALSE;
}

LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* /*info*/) {
    write_terminal_reset();
    return EXCEPTION_CONTINUE_SEARCH;
}

#else

void posix_fatal_signal_handler(int sig) {
    write_terminal_reset();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

#endif

} // namespace

void write_terminal_reset() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != INVALID_HANDLE_VALUE && out != nullptr) {
        DWORD written = 0;
        WriteFile(out, kResetSequence, static_cast<DWORD>(kResetSequenceLen), &written, nullptr);
    }
#else
    ssize_t ignored = ::write(STDOUT_FILENO, kResetSequence, kResetSequenceLen);
    (void)ignored;
#endif
}

void install_crash_guard() {
    std::set_terminate(previous_terminate_and_abort);

#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    SetUnhandledExceptionFilter(unhandled_exception_filter);
#else
    for (int sig : {SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL}) {
        std::signal(sig, posix_fatal_signal_handler);
    }
#endif
}

} // namespace alyssa_safety
