// test_personality.cpp
// Unit tests for PersonalityCore.hpp (loading, hormone-modulated state,
// prompt block rendering). Header-only target — no model, no llama.cpp.

#include "../includes/PersonalityCore.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

using namespace alyssa_personality;
using alyssa_endocrine::HormoneProfile;

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

static const char* TEST_PROFILE_PATH = "test_personality.json";

static void write_test_profile() {
    std::ofstream f(TEST_PROFILE_PATH);
    f << R"({
        "name": "Alyssa",
        "identity": "Garota de teste.",
        "traits": { "friendliness": 0.8, "curiosity": 0.9, "sass": 0.6, "patience": 0.5 },
        "interests": ["tecnologia", "jogos"],
        "speech": {
            "style_rules": ["informal", "respostas curtas"],
            "emoji_frequency": 0.15
        }
    })";
}

// =============================================================================
// load_personality
// =============================================================================
static void test_loading() {
    Personality missing = load_personality("nao_existe.json");
    TEST("missing file -> not loaded",     !missing.loaded);
    TEST("missing file -> empty context",  generate_personality_context(missing, nullptr).empty());

    write_test_profile();
    Personality p = load_personality(TEST_PROFILE_PATH);
    TEST("profile loads",                  p.loaded);
    TEST("name loaded",                    p.name == "Alyssa");
    TEST("identity loaded",                p.identity == "Garota de teste.");
    TEST("traits loaded",                  p.traits.size() == 4);
    TEST("trait accessor",                 p.trait("sass") > 0.59 && p.trait("sass") < 0.61);
    TEST("trait fallback",                 p.trait("inexistente", 0.3) == 0.3);
    TEST("interests loaded",               p.interests.size() == 2);
    TEST("style rules loaded",             p.style_rules.size() == 2);
    TEST("emoji frequency loaded",         p.emoji_frequency > 0.14 && p.emoji_frequency < 0.16);

    std::ofstream bad("test_bad_personality.json");
    bad << "{ isso nao é json";
    bad.close();
    Personality broken = load_personality("test_bad_personality.json");
    TEST("malformed json -> not loaded",   !broken.loaded);
    std::remove("test_bad_personality.json");
}

// =============================================================================
// derive_current_state (hormone modulation)
// =============================================================================
static void test_state_derivation() {
    write_test_profile();
    Personality p = load_personality(TEST_PROFILE_PATH);

    TEST("null profile -> empty state",    derive_current_state(p, nullptr).empty());

    HormoneProfile baseline; // valores default do construtor
    std::string state = derive_current_state(p, &baseline);
    TEST("baseline has energy descriptor", state.find("energia") != std::string::npos);
    TEST("baseline has mood descriptor",   state.find("humor") != std::string::npos);

    HormoneProfile stressed;
    stressed.cortisol = 0.9;
    stressed.serotonin = 0.2;
    state = derive_current_state(p, &stressed);
    TEST("high cortisol -> short patience", state.find("paciência curta") != std::string::npos);
    TEST("stressed -> unstable mood",       state.find("instável") != std::string::npos);

    HormoneProfile energized;
    energized.dopamine = 0.9;
    energized.adrenaline = 0.8;
    state = derive_current_state(p, &energized);
    TEST("high dopamine+adrenaline -> high energy", state.find("energia alta") != std::string::npos);
    TEST("adrenaline boosts sass",          state.find("respondona") != std::string::npos);

    HormoneProfile social;
    social.oxytocin = 0.8;
    social.serotonin = 0.7;
    state = derive_current_state(p, &social);
    TEST("high oxytocin -> affectionate",   state.find("carinhosa") != std::string::npos);
    TEST("high serotonin -> good mood",     state.find("bom humor") != std::string::npos);
}

// =============================================================================
// generate_personality_context
// =============================================================================
static void test_context_rendering() {
    write_test_profile();
    Personality p = load_personality(TEST_PROFILE_PATH);

    HormoneProfile h;
    std::string block = generate_personality_context(p, &h);

    TEST("block has open tag",             block.find("[PERSONALIDADE]") != std::string::npos);
    TEST("block has close tag",            block.find("[/PERSONALIDADE]") != std::string::npos);
    TEST("block has identity",             block.find("Garota de teste.") != std::string::npos);
    TEST("block has interests",            block.find("tecnologia, jogos") != std::string::npos);
    TEST("block has style rules",          block.find("informal; respostas curtas") != std::string::npos);
    TEST("block has current state",        block.find("Agora: ") != std::string::npos);

    std::string no_hormones = generate_personality_context(p, nullptr);
    TEST("no hormones -> no hormone descriptors", no_hormones.find("energia") == std::string::npos);
    TEST("no hormones -> still has period",  no_hormones.find("Agora: ") != std::string::npos);
    TEST("no hormones -> still has identity", no_hormones.find("Garota de teste.") != std::string::npos);

    // Período do dia (função pura)
    TEST("3h é madrugada",                 period_of_day(3) == "madrugada");
    TEST("9h é manhã",                     period_of_day(9) == "manhã");
    TEST("15h é tarde",                    period_of_day(15) == "tarde");
    TEST("21h é noite",                    period_of_day(21) == "noite");
    TEST("0h é madrugada",                 period_of_day(0) == "madrugada");
    TEST("23h é noite",                    period_of_day(23) == "noite");

    // Bloco precisa ser compacto (modelos pequenos): < 600 chars com este perfil
    TEST("block stays compact",            block.size() < 600);
}

int main() {
    test_loading();
    test_state_derivation();
    test_context_rendering();

    std::remove(TEST_PROFILE_PATH);

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << tests_passed << "  Failed: " << tests_failed << "\n";
    std::cout << "========================================\n";
    return tests_failed == 0 ? 0 : 1;
}
