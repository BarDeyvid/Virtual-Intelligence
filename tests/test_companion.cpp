// test_companion.cpp
// Unit tests for the companion features: TicTacToe game logic and UserPrefs
// persistence. Header-only targets, no model, no OpenCV.

#include "../includes/TicTacToe.hpp"
#include "../includes/UserPrefs.hpp"
#include "../includes/InputController.hpp"
#include <cstdio>
#include <iostream>

using namespace alyssa_games;
using namespace alyssa_prefs;

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

// =============================================================================
// TicTacToe
// =============================================================================
static void test_tictactoe_basics() {
    TicTacToe game;
    TEST("new game in progress",           game.status() == TicTacToe::Status::InProgress);

    TEST("invalid cell rejected",          !game.play(0).empty());
    TEST("invalid cell 10 rejected",       !game.play(10).empty());

    TEST("valid move accepted",            game.play(1).empty());
    TEST("user mark placed",               game.cell_at(1) == 'X');
    TEST("engine responded",               game.cell_at(5) == 'O'); // engine pega o centro

    TEST("occupied cell rejected",         !game.play(1).empty());
    TEST("engine cell rejected",           !game.play(5).empty());

    game.reset();
    TEST("reset clears board",             game.cell_at(1) == ' ' && game.cell_at(5) == ' ');
}

static void test_tictactoe_engine_blocks() {
    // Usuário ameaça 1-2-3; engine tem que bloquear a casa 3
    TicTacToe game;
    game.play(1); // X:1, O:5 (centro)
    game.play(2); // X:2 → ameaça em 3; engine deve bloquear
    TEST("engine blocks winning line",     game.cell_at(3) == 'O');
}

static void test_tictactoe_engine_wins() {
    // Deixa a engine montar linha e confirmar que ela fecha quando pode.
    // X: 1, 2 (bloqueada em 3), depois X joga longe e deixa O vencer.
    TicTacToe game;
    game.play(1); // O:5
    game.play(2); // O bloqueia em 3
    game.play(7); // X:7 ameaça 1-4-7? 4 tá livre... engine decide
    // Independente da linha exata, o jogo deve continuar válido
    TEST("game still valid",               game.status() == TicTacToe::Status::InProgress ||
                                           game.status() == TicTacToe::Status::AlyssaWon);
}

static void test_tictactoe_user_wins() {
    // Tabuleiro manipulado por jogadas: usuário fecha coluna 1-4-7 se a
    // engine não bloquear duas ameaças simultâneas (fork clássico)
    TicTacToe game;
    game.play(1); // X:1, O:5
    game.play(9); // X:9 (fork diagonal impossível, O:5 já tem centro), O responde
    game.play(3); // pode criar dupla ameaça 1-2-3 e 3-6-9
    // Só valida consistência: nunca dois status ao mesmo tempo, tabuleiro legal
    int x_count = 0, o_count = 0;
    for (int i = 1; i <= 9; ++i) {
        if (game.cell_at(i) == 'X') ++x_count;
        if (game.cell_at(i) == 'O') ++o_count;
    }
    TEST("board is consistent",            x_count >= o_count && x_count - o_count <= 1);
}

static void test_tictactoe_draw() {
    // Sequência que força velha contra a engine determinística:
    TicTacToe game;
    game.play(1); // O:5
    game.play(2); // O:3 (bloqueia 1-2-3)
    game.play(7); // O:4 (bloqueia 1-4-7)
    game.play(6); // O:9? engine fecha/bloqueia conforme linhas
    // joga o que sobrar até acabar
    for (int cell = 1; cell <= 9 && game.status() == TicTacToe::Status::InProgress; ++cell) {
        if (game.cell_at(cell) == ' ') game.play(cell);
    }
    TEST("game ends",                      game.status() != TicTacToe::Status::InProgress);
    TEST("user never wins vs blocker here", game.status() != TicTacToe::Status::UserWon);
}

static void test_tictactoe_render() {
    TicTacToe game;
    game.play(1);
    std::string board = game.render();
    TEST("render has X",                   board.find('X') != std::string::npos);
    TEST("render has O",                   board.find('O') != std::string::npos);
    TEST("render shows free cells",        board.find('9') != std::string::npos);
    TEST("status text in progress",        game.status_text().find("andamento") != std::string::npos);
}

// =============================================================================
// UserPrefs
// =============================================================================
static const char* TEST_PREFS = "test_user_prefs.json";

static void test_prefs() {
    std::remove(TEST_PREFS);

    TEST("empty file -> no prefs",         load_preferences(TEST_PREFS).empty());
    TEST("empty file -> empty line",       render_preferences_line(TEST_PREFS).empty());

    TEST("add preference works",           add_preference("música", "phonk", TEST_PREFS));
    TEST("add second preference",          add_preference("jogo", "Hollow Knight", TEST_PREFS));

    auto prefs = load_preferences(TEST_PREFS);
    TEST("two prefs stored",               prefs.size() == 2);
    TEST("category stored",                prefs[0].category == "música");
    TEST("value stored",                   prefs[0].value == "phonk");
    TEST("date stored",                    prefs[0].learned_at.size() == 10);

    // Dedup por valor
    TEST("duplicate updates category",     add_preference("som", "phonk", TEST_PREFS));
    prefs = load_preferences(TEST_PREFS);
    TEST("no duplicate created",           prefs.size() == 2);
    TEST("category updated on dup",        prefs[0].category == "som");

    std::string line = render_preferences_line(TEST_PREFS);
    TEST("line lists values",              line.find("phonk") != std::string::npos &&
                                           line.find("Hollow Knight") != std::string::npos);
    TEST("empty value rejected",           !add_preference("x", "", TEST_PREFS));

    // Arquivo corrompido: degrada pra vazio sem explodir
    std::ofstream bad(TEST_PREFS);
    bad << "{ lixo";
    bad.close();
    TEST("corrupted file -> empty",        load_preferences(TEST_PREFS).empty());

    std::remove(TEST_PREFS);
}

// =============================================================================
// InputController::parse_key_combo (pura — não envia eventos)
// =============================================================================
static void test_key_combo_parsing() {
    using alyssa_input::InputController;

    TEST("single key parses",              InputController::parse_key_combo("enter").size() == 1);
    TEST("combo parses two keys",          InputController::parse_key_combo("ctrl+s").size() == 2);
    TEST("triple combo parses",            InputController::parse_key_combo("ctrl+shift+a").size() == 3);
    TEST("case insensitive",               InputController::parse_key_combo("CTRL+S").size() == 2);
    TEST("spaces tolerated",               InputController::parse_key_combo("ctrl + s").size() == 2);
    TEST("letter key parses",              InputController::parse_key_combo("a").size() == 1);
    TEST("digit key parses",               InputController::parse_key_combo("5").size() == 1);
    TEST("portuguese alias",               InputController::parse_key_combo("espaco").size() == 1);
    TEST("unknown key rejects combo",      InputController::parse_key_combo("ctrl+trem").empty());
    TEST("garbage rejected",               InputController::parse_key_combo("çãõ").empty());
}

int main() {
    test_key_combo_parsing();
    test_tictactoe_basics();
    test_tictactoe_engine_blocks();
    test_tictactoe_engine_wins();
    test_tictactoe_user_wins();
    test_tictactoe_draw();
    test_tictactoe_render();
    test_prefs();

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << tests_passed << "  Failed: " << tests_failed << "\n";
    std::cout << "========================================\n";
    return tests_failed == 0 ? 0 : 1;
}
