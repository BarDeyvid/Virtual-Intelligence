// InputController.cpp
// SendInput-based keyboard/mouse control (Windows). Stubs elsewhere.

#include "InputController.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace alyssa_input {

// =============================================================================
// Mapa de teclas (nome PT/EN → código). No Windows são VK codes; nos stubs os
// valores são simbólicos (só o parse é usado nos testes).
// =============================================================================

static const std::map<std::string, int>& key_map() {
#ifdef _WIN32
    static const std::map<std::string, int> keys = {
        {"enter", VK_RETURN}, {"esc", VK_ESCAPE}, {"escape", VK_ESCAPE},
        {"tab", VK_TAB}, {"space", VK_SPACE}, {"espaco", VK_SPACE},
        {"backspace", VK_BACK}, {"delete", VK_DELETE}, {"del", VK_DELETE},
        {"home", VK_HOME}, {"end", VK_END},
        {"pageup", VK_PRIOR}, {"pagedown", VK_NEXT},
        {"up", VK_UP}, {"down", VK_DOWN}, {"left", VK_LEFT}, {"right", VK_RIGHT},
        {"cima", VK_UP}, {"baixo", VK_DOWN}, {"esquerda", VK_LEFT}, {"direita", VK_RIGHT},
        {"ctrl", VK_CONTROL}, {"alt", VK_MENU}, {"shift", VK_SHIFT}, {"win", VK_LWIN},
        {"f1", VK_F1}, {"f2", VK_F2}, {"f3", VK_F3}, {"f4", VK_F4},
        {"f5", VK_F5}, {"f6", VK_F6}, {"f7", VK_F7}, {"f8", VK_F8},
        {"f9", VK_F9}, {"f10", VK_F10}, {"f11", VK_F11}, {"f12", VK_F12},
    };
#else
    static const std::map<std::string, int> keys = {
        {"enter", 1}, {"esc", 2}, {"tab", 3}, {"space", 4},
        {"ctrl", 5}, {"alt", 6}, {"shift", 7},
        {"up", 8}, {"down", 9}, {"left", 10}, {"right", 11},
    };
#endif
    return keys;
}

std::vector<int> InputController::parse_key_combo(const std::string& combo) {
    std::vector<int> codes;
    std::stringstream ss(combo);
    std::string part;

    while (std::getline(ss, part, '+')) {
        // normaliza: minúsculas, sem espaços
        part.erase(std::remove_if(part.begin(), part.end(),
                                  [](unsigned char c) { return std::isspace(c); }),
                   part.end());
        std::transform(part.begin(), part.end(), part.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (part.empty()) continue;

        auto it = key_map().find(part);
        if (it != key_map().end()) {
            codes.push_back(it->second);
        } else if (part.size() == 1 &&
                   (std::isalnum(static_cast<unsigned char>(part[0])))) {
            // letras e dígitos: VK == ASCII maiúsculo no Windows
            codes.push_back(std::toupper(static_cast<unsigned char>(part[0])));
        } else {
            return {}; // tecla desconhecida invalida o combo inteiro
        }
    }
    return codes;
}

// =============================================================================
// Windows implementation
// =============================================================================
#ifdef _WIN32

std::string InputController::screen_info() {
    int width  = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    POINT cursor{};
    GetCursorPos(&cursor);
    return "Tela: " + std::to_string(width) + "x" + std::to_string(height) +
           ". Cursor em (" + std::to_string(cursor.x) + ", " + std::to_string(cursor.y) + ").";
}

std::string InputController::move_mouse(int x, int y) {
    int width  = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    x = std::clamp(x, 0, width - 1);
    y = std::clamp(y, 0, height - 1);

    if (!SetCursorPos(x, y)) {
        return "ERRO: falha ao mover o cursor";
    }
    return "Cursor movido para (" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

static void send_click(DWORD down_flag, DWORD up_flag) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = down_flag;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = up_flag;
    SendInput(2, inputs, sizeof(INPUT));
}

std::string InputController::click(const std::string& button) {
    if (button == "right" || button == "direito") {
        send_click(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP);
        return "Clique direito enviado";
    }
    if (button == "double" || button == "duplo") {
        send_click(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP);
        Sleep(50);
        send_click(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP);
        return "Clique duplo enviado";
    }
    if (button == "left" || button == "esquerdo" || button.empty()) {
        send_click(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP);
        return "Clique esquerdo enviado";
    }
    return "ERRO: botão desconhecido '" + button + "' (use left, right ou double)";
}

std::string InputController::type_text(const std::string& text) {
    if (text.empty()) return "ERRO: texto vazio";
    if (text.size() > 300) return "ERRO: texto longo demais (máx. 300 caracteres por chamada)";

    // UTF-8 → UTF-16 para eventos KEYEVENTF_UNICODE (acentos pt-BR funcionam)
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                       static_cast<int>(text.size()), nullptr, 0);
    if (wide_len <= 0) return "ERRO: falha ao converter texto";
    std::wstring wide(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                        static_cast<int>(text.size()), wide.data(), wide_len);

    std::vector<INPUT> inputs;
    inputs.reserve(wide.size() * 2);
    for (wchar_t wc : wide) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = wc;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    if (sent != inputs.size()) return "ERRO: nem todos os eventos foram enviados";
    return "Texto digitado (" + std::to_string(wide.size()) + " caracteres)";
}

std::string InputController::press_key(const std::string& combo) {
    std::vector<int> codes = parse_key_combo(combo);
    if (codes.empty()) {
        return "ERRO: tecla/combo desconhecido '" + combo +
               "' (exemplos: enter, esc, ctrl+s, alt+tab, f5)";
    }

    std::vector<INPUT> inputs;
    // pressiona na ordem, solta na ordem inversa (comportamento de combo)
    for (int vk : codes) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        inputs.push_back(in);
    }
    for (auto it = codes.rbegin(); it != codes.rend(); ++it) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(*it);
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(in);
    }

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    if (sent != inputs.size()) return "ERRO: nem todos os eventos foram enviados";
    return "Tecla(s) pressionada(s): " + combo;
}

// =============================================================================
// Stubs (Linux/macOS)
// =============================================================================
#else

static std::string not_supported() {
    return "ERRO: controle de teclado/mouse só está implementado no Windows por enquanto";
}

std::string InputController::screen_info()                        { return not_supported(); }
std::string InputController::move_mouse(int, int)                 { return not_supported(); }
std::string InputController::click(const std::string&)            { return not_supported(); }
std::string InputController::type_text(const std::string&)        { return not_supported(); }
std::string InputController::press_key(const std::string&)        { return not_supported(); }

#endif

} // namespace alyssa_input
