/**
 * @file InputController.hpp
 * @brief Controle de teclado/mouse para a Alyssa (companion mode).
 *
 * Dá "mãos" pra Alyssa via SendInput no Windows: mover o mouse, clicar,
 * digitar texto (Unicode, acentos ok) e apertar teclas/combos. Exposto como
 * tools (mouse_move, mouse_click, keyboard_type, keyboard_key, screen_info) —
 * remover a entrada do tools_registry.json desativa o controle.
 *
 * Rails de segurança:
 *  - coordenadas clampadas à tela
 *  - texto limitado a 300 chars por chamada
 *  - combos só com teclas conhecidas do mapa (nome inválido = erro, não crash)
 *
 * Linux/macOS: stub que retorna erro amigável (ydotool/xdotool ficam pra
 * quando o projeto voltar pro Hyprland).
 */

#pragma once

#include <string>
#include <vector>

namespace alyssa_input {

class InputController {
public:
    /// Resolução da tela + posição atual do cursor (contexto pra mirar).
    static std::string screen_info();

    /// Move o cursor para (x, y) absoluto. Clampa aos limites da tela.
    static std::string move_mouse(int x, int y);

    /// Clique: "left" (default), "right" ou "double".
    static std::string click(const std::string& button);

    /// Digita texto como eventos Unicode (máx. 300 chars).
    static std::string type_text(const std::string& text);

    /// Pressiona tecla ou combo ("enter", "esc", "ctrl+s", "alt+tab"...).
    static std::string press_key(const std::string& combo);

    /**
     * @brief Resolve um combo "ctrl+shift+s" em códigos de tecla (VK no Windows).
     * @return Vetor vazio quando alguma tecla é desconhecida. Pura → testável.
     */
    static std::vector<int> parse_key_combo(const std::string& combo);
};

} // namespace alyssa_input
