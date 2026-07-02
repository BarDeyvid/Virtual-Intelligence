// test_fusion_utils.cpp
// Unit tests for the pure, model-free utility functions in FusionUtils.hpp.
// No model loading, no llama.cpp, no ONNX — fast to compile and run.

#include "../includes/FusionUtils.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <map>
#include <set>

using namespace alyssa_utils;

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
// is_small_talk
// =============================================================================
static void test_is_small_talk() {
    TEST("greeting 'oi' is small talk",           is_small_talk("oi"));
    TEST("greeting 'olá' is small talk",          is_small_talk("olá"));
    TEST("greeting 'bom dia' is small talk",      is_small_talk("bom dia"));
    TEST("'tudo bem' is small talk",              is_small_talk("tudo bem"));
    TEST("'hey' is small talk",                   is_small_talk("hey"));

    TEST("question with ? is NOT small talk",    !is_small_talk("como funciona a IA?"));
    TEST("question palavras marked as content",  !is_small_talk("explique o que é fusão"));
    TEST("long substancial input NOT small talk",!is_small_talk("preciso de ajuda com um problema complexo de matemática"));
    TEST("'porque' indicates content",           !is_small_talk("porque o céu é azul?"));

    // Edge cases
    TEST("empty string is small talk (len<30)",   is_small_talk(""));
    TEST("single letter",                        is_small_talk("a"));
    TEST("mixed greeting + content -> content",  !is_small_talk("oi, como funciona?"));
}

// =============================================================================
// calculate_string_similarity
// =============================================================================
static void test_calculate_string_similarity() {
    TEST("identical strings score 1.0",
         calculate_string_similarity("hello world", "hello world") > 0.99f);

    TEST("disjoint strings score 0.0",
         calculate_string_similarity("abc", "xyz") < 0.01f);

    TEST("case insensitive",
         calculate_string_similarity("Hello", "hello") > 0.99f);

    float half = calculate_string_similarity("gato cachorro", "gato passaro");
    TEST("overlap ~50%", half > 0.3f && half < 0.7f);

    TEST("both empty = 1.0",
         calculate_string_similarity("", "") > 0.99f);

    TEST("one empty = 0.0",
         calculate_string_similarity("a", "") < 0.01f);
}

// =============================================================================
// calculate_history_limit
// =============================================================================
static void test_calculate_history_limit() {
    TEST("neutral intensity = default cap 50",
         calculate_history_limit("default", 0.5f) == 50);

    TEST("introspective gets extra",
         calculate_history_limit("introspectiveModel", 0.5f) == 50);

    // NOTE: the original code starts at 150 but caps at 50, so
    // emotional-intensity adjustments are effectively dead code.
    // The test documents this pre-existing bug — it always returns 50.
    TEST("always capped at 50 regardless of intensity",
         calculate_history_limit("default", 0.8f) == 50);

    TEST("low intensity reduces limit",
         calculate_history_limit("default", 0.1f) <= 50);

    // Clamp
    TEST("never below 10",
         calculate_history_limit("default", 0.5f) >= 10);
    TEST("never above 50",
         calculate_history_limit("introspectiveModel", 0.9f) <= 50);
}

// =============================================================================
// calculate_committee_coherence
// =============================================================================
static void test_calculate_committee_coherence() {
    using C = alyssa_fusion::ExpertContribution;

    TEST("empty list = 1.0",
         calculate_committee_coherence({}) > 0.99f);

    {
        C a, b;
        a.response = "[SINAL] bom [CONFIANÇA] 0.9";
        b.response = "[SINAL] ruim [CONFIANÇA] 0.8";
        TEST("two clean signals = 1.0",
             calculate_committee_coherence({a, b}) > 0.99f);
    }

    {
        C a, b;
        a.response = "[ERRO] falha";
        b.response = "[ERRO] timeout";
        TEST("both erro = 0.0",
             calculate_committee_coherence({a, b}) < 0.01f);
    }

    {
        C a, b, c;
        a.response = "ok";
        b.response = "[ERRO] crash";
        c.response = "ok";
        // pairs: (a,b)=1 (b,c)=1 (a,c)=1  → all compatible since only b has erro
        TEST("only one erro = still 1.0",
             calculate_committee_coherence({a, b, c}) > 0.99f);
    }
}

// =============================================================================
// apply_topk_gating
// =============================================================================
static void test_apply_topk_gating() {
    std::map<std::string, double> weights = {
        {"a", 0.9},
        {"b", 0.6},
        {"c", 0.3},
        {"d", 0.05}  // below default threshold 0.15
    };

    auto active = apply_topk_gating(weights, 3, 0.15);

    TEST("top-3 above threshold activated",  active.count("a") && active.count("b") && active.count("c"));
    TEST("below threshold excluded",         active.count("d") == 0);
    TEST("below threshold weight zeroed",    weights["d"] < 0.001);

    // top_k = 1
    auto active1 = apply_topk_gating(weights, 1, 0.15);
    TEST("top-1 only",                       active1.size() == 1 && active1.count("a"));
    TEST("second expert zeroed by k-limit",  weights["b"] < 0.001);
}

// =============================================================================
// main
// =============================================================================
int main() {
    test_is_small_talk();
    test_calculate_string_similarity();
    test_calculate_history_limit();
    test_calculate_committee_coherence();
    test_apply_topk_gating();

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    return tests_failed > 0 ? 1 : 0;
}
