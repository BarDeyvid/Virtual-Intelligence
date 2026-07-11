// TerminalSafety.hpp
#pragma once

namespace alyssa_safety {

/**
 * @brief Installs a best-effort terminal-restore net for FTXUI's alternate
 *        screen / hidden-cursor mode.
 *
 * FTXUI's ScreenInteractive only restores the terminal (exits the alternate
 * screen buffer, shows the cursor again) on a clean shutdown path. If any
 * thread throws an uncaught exception, std::terminate() kills the process
 * before that cleanup ever runs, leaving raw ANSI escape sequences bleeding
 * into the terminal after exit.
 *
 * This does NOT prevent a crash — it just makes sure the terminal-reset
 * sequence gets written first, via:
 *   - std::set_terminate(): catches uncaught C++ exceptions.
 *   - SetUnhandledExceptionFilter() (Windows) / a SIGSEGV/SIGABRT/SIGILL/
 *     SIGFPE handler (POSIX): catches hard crashes, on a best-effort basis
 *     (a real memory-corruption crash can still be too far gone for this to
 *     run safely; this covers the much more common uncaught-exception case).
 *   - SetConsoleCtrlHandler() (Windows) / SIGINT/SIGTERM (POSIX): catches
 *     Ctrl+C, console-close, and logoff/shutdown before the OS tears the
 *     process down.
 *
 * Call once, near the top of main(), before ScreenInteractive touches the
 * terminal.
 */
void install_crash_guard();

/// Writes the raw terminal-reset sequence (show cursor, exit alt-screen,
/// reset attributes) directly via the OS write syscall — no iostream, no
/// allocation, safe to call from a signal/exception handler context.
void write_terminal_reset();

} // namespace alyssa_safety
