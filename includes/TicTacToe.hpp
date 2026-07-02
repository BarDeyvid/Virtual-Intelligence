/**
 * @file TicTacToe.hpp
 * @brief Jogo da velha com lógica em C++ (companion feature).
 *
 * A Alyssa "joga" chamando a tool jogo_da_velha — toda a regra fica aqui,
 * então o modelo 4B não precisa ser bom em jogo da velha (nem consegue
 * trapacear). O usuário é X, a Alyssa é O. Header-only e puro: testável sem
 * modelo.
 *
 * Estratégia da engine (determinística): vencer > bloquear > centro > canto >
 * lateral. Forte o suficiente pra nunca perder de bobeira, mas sem minimax —
 * dá pro usuário ganhar se a abertura ajudar.
 */

#pragma once

#include <array>
#include <string>

namespace alyssa_games {

class TicTacToe {
public:
    enum class Status { InProgress, UserWon, AlyssaWon, Draw };

    TicTacToe() { reset(); }

    void reset() {
        board.fill(' ');
        status_ = Status::InProgress;
    }

    Status status() const { return status_; }

    /**
     * @brief Aplica a jogada do usuário (X) e responde com a da Alyssa (O).
     * @param cell Casa 1-9 (linha a linha).
     * @return Mensagem de erro, ou "" quando a jogada foi aceita.
     */
    std::string play(int cell) {
        if (status_ != Status::InProgress) {
            return "o jogo já acabou — use jogada=nova para recomeçar";
        }
        if (cell < 1 || cell > 9) {
            return "casa inválida: use um número de 1 a 9";
        }
        if (board[cell - 1] != ' ') {
            return "a casa " + std::to_string(cell) + " já está ocupada";
        }

        board[cell - 1] = 'X';
        update_status();
        if (status_ != Status::InProgress) return "";

        int alyssa_cell = choose_engine_move();
        if (alyssa_cell >= 0) {
            board[alyssa_cell] = 'O';
            update_status();
        }
        return "";
    }

    /// Tabuleiro em ASCII com números nas casas livres.
    std::string render() const {
        std::string out;
        for (int row = 0; row < 3; ++row) {
            out += " ";
            for (int col = 0; col < 3; ++col) {
                int i = row * 3 + col;
                out += board[i] == ' ' ? std::to_string(i + 1) : std::string(1, board[i]);
                if (col < 2) out += " | ";
            }
            out += "\n";
            if (row < 2) out += "-----------\n";
        }
        return out;
    }

    std::string status_text() const {
        switch (status_) {
            case Status::UserWon:   return "O usuário VENCEU!";
            case Status::AlyssaWon: return "Alyssa VENCEU!";
            case Status::Draw:      return "Deu velha (empate)!";
            default:                return "Jogo em andamento. Usuário é X, Alyssa é O.";
        }
    }

    // Exposto para testes
    char cell_at(int cell) const { return board[cell - 1]; }

private:
    std::array<char, 9> board;
    Status status_ = Status::InProgress;

    static constexpr int WIN_LINES[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},   // linhas
        {0,3,6},{1,4,7},{2,5,8},   // colunas
        {0,4,8},{2,4,6}            // diagonais
    };

    void update_status() {
        for (const auto& line : WIN_LINES) {
            char a = board[line[0]];
            if (a != ' ' && a == board[line[1]] && a == board[line[2]]) {
                status_ = (a == 'X') ? Status::UserWon : Status::AlyssaWon;
                return;
            }
        }
        for (char c : board) {
            if (c == ' ') return; // ainda tem espaço
        }
        status_ = Status::Draw;
    }

    /// Casa que completa uma linha para 'player', ou -1.
    int find_winning_cell(char player) const {
        for (const auto& line : WIN_LINES) {
            int empty = -1, count = 0;
            for (int idx : line) {
                if (board[idx] == player) ++count;
                else if (board[idx] == ' ') empty = idx;
            }
            if (count == 2 && empty >= 0) return empty;
        }
        return -1;
    }

    int choose_engine_move() const {
        // 1. Vencer se der
        int cell = find_winning_cell('O');
        if (cell >= 0) return cell;
        // 2. Bloquear o usuário
        cell = find_winning_cell('X');
        if (cell >= 0) return cell;
        // 3. Centro
        if (board[4] == ' ') return 4;
        // 4. Cantos, 5. Laterais
        for (int i : {0, 2, 6, 8, 1, 3, 5, 7}) {
            if (board[i] == ' ') return i;
        }
        return -1; // tabuleiro cheio
    }
};

} // namespace alyssa_games
