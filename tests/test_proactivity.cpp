// test_proactivity.cpp
// Unit tests for ProactivityEngine.hpp. All time-dependent behavior uses the
// injectable clock — deterministic, zero sleeps, no model loading.

#include "../includes/ProactivityEngine.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>

using namespace alyssa_proactivity;
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

static ProactivityConfig default_cfg() {
    ProactivityConfig cfg;
    cfg.enabled = true;
    cfg.boredom_threshold_s = 300;
    cfg.min_idle_for_mood_s = 120;
    cfg.cooldown_s = 600;
    cfg.startup_grace_s = 600; // == cooldown → sem retrodatação (semântica original dos testes)
    cfg.max_cortisol_for_boredom = 0.7;
    cfg.stress_cortisol_threshold = 0.65;
    cfg.excitement_dopamine_threshold = 0.75;
    return cfg;
}

static TimePoint t0() { return TimePoint{}; } // época zero do steady_clock
static TimePoint at(long long s) { return t0() + std::chrono::seconds(s); }

// =============================================================================
// config loading
// =============================================================================
static void test_config_loading() {
    ProactivityConfig missing = load_proactivity_config("nao_existe.json");
    TEST("missing config uses defaults",   missing.boredom_threshold_s == 300 && missing.enabled);

    std::ofstream f("test_proactivity_cfg.json");
    f << R"({ "enabled": false, "boredom_threshold_s": 60, "cooldown_s": 30 })";
    f.close();
    ProactivityConfig cfg = load_proactivity_config("test_proactivity_cfg.json");
    TEST("config values loaded",           !cfg.enabled && cfg.boredom_threshold_s == 60 && cfg.cooldown_s == 30);
    TEST("unset fields keep defaults",     cfg.min_idle_for_mood_s == 120);
    std::remove("test_proactivity_cfg.json");

    std::ofstream bad("test_proactivity_bad.json");
    bad << "{ nem json isso";
    bad.close();
    ProactivityConfig broken = load_proactivity_config("test_proactivity_bad.json");
    TEST("malformed config uses defaults", broken.enabled && broken.boredom_threshold_s == 300);
    std::remove("test_proactivity_bad.json");
}

// =============================================================================
// boredom trigger
// =============================================================================
static void test_boredom() {
    ProactivityEngine engine(default_cfg(), t0());
    HormoneProfile calm; // cortisol 0.2 default

    TEST("nothing fires immediately",      engine.check(calm, at(10)).type == TriggerType::None);
    TEST("cooldown blocks early boredom",  engine.check(calm, at(400)).type == TriggerType::None);

    // Passado o cooldown (600s desde o boot) e o threshold de tédio (300s idle)
    auto trig = engine.check(calm, at(700));
    TEST("boredom fires after cooldown+idle", trig.type == TriggerType::Boredom);
    TEST("boredom reason mentions minutes",   trig.reason.find("11 minuto(s)") != std::string::npos);

    // Atividade do usuário reseta o idle
    engine.note_user_activity(at(700));
    TEST("activity resets idle",           engine.check(calm, at(900)).type == TriggerType::None);
    TEST("idle_seconds tracks activity",   engine.idle_seconds(at(760)) == 60);

    // Idle de novo além do threshold
    TEST("boredom fires again after idle", engine.check(calm, at(1001)).type == TriggerType::Boredom);

    // Cortisol alto bloqueia tédio (sem clima pra descontração)
    HormoneProfile stressed_but_low; // abaixo do stress_threshold, acima do max p/ tédio
    stressed_but_low.cortisol = 0.72;
    stressed_but_low.dopamine = 0.3;
    // 0.72 > 0.65 → vira StressCheck, não Boredom
    TEST("high cortisol never bores",      engine.check(stressed_but_low, at(1001)).type != TriggerType::Boredom);
}

// =============================================================================
// cooldown
// =============================================================================
static void test_cooldown() {
    ProactivityEngine engine(default_cfg(), t0());
    HormoneProfile calm;

    auto trig = engine.check(calm, at(700));
    TEST("first trigger fires",            trig.type == TriggerType::Boredom);
    engine.note_proactive_message(at(700));

    TEST("cooldown blocks repeat",         engine.check(calm, at(900)).type == TriggerType::None);
    TEST("cooldown expires eventually",    engine.check(calm, at(1301)).type == TriggerType::Boredom);
    TEST("seconds_since_proactive",        engine.seconds_since_proactive(at(760)) == 60);
}

// =============================================================================
// mood triggers (stress / excitement)
// =============================================================================
static void test_mood_triggers() {
    ProactivityEngine engine(default_cfg(), t0());

    HormoneProfile stressed;
    stressed.cortisol = 0.8;

    // Cooldown ok (700 > 600), idle 700 > min_idle 120, cortisol bloqueia tédio → stress check
    auto trig = engine.check(stressed, at(700));
    TEST("stress check fires",             trig.type == TriggerType::StressCheck);
    TEST("stress reason asks about user",  trig.reason.find("tudo bem") != std::string::npos);

    // Usuário ativo há pouco → gatilho de humor não interrompe conversa
    ProactivityEngine engine2(default_cfg(), t0());
    engine2.note_user_activity(at(650));
    TEST("mood needs min idle",            engine2.check(stressed, at(700)).type == TriggerType::None);

    // Dopamina alta → sugestão de atividade
    HormoneProfile excited;
    excited.dopamine = 0.9;
    excited.cortisol = 0.1;
    ProactivityEngine engine3(default_cfg(), t0());
    engine3.note_user_activity(at(500));
    // idle = 200 (>= 120 p/ humor, < 300 p/ tédio) → Excitement
    trig = engine3.check(excited, at(700));
    TEST("excitement fires on high dopamine", trig.type == TriggerType::Excitement);

    // Com idle >= boredom_threshold e cortisol baixo, tédio tem prioridade
    trig = engine3.check(excited, at(801));
    TEST("boredom outranks excitement",    trig.type == TriggerType::Boredom);
}

// =============================================================================
// startup grace (primeira mensagem antes do cooldown cheio)
// =============================================================================
static void test_startup_grace() {
    auto cfg = default_cfg();
    cfg.startup_grace_s = 120;
    cfg.boredom_threshold_s = 60;
    ProactivityEngine engine(cfg, t0());
    HormoneProfile calm;

    // Idle já passou do threshold (100 > 60), mas grace de 120s ainda não venceu
    TEST("grace blocks before 120s",       engine.check(calm, at(100)).type == TriggerType::None);

    // Grace vencida: primeira mensagem liberada bem antes do cooldown de 600s
    TEST("first message after grace",      engine.check(calm, at(125)).type == TriggerType::Boredom);
    engine.note_proactive_message(at(125));

    // Entre mensagens vale o cooldown CHEIO
    TEST("full cooldown after first",      engine.check(calm, at(600)).type == TriggerType::None);
    TEST("second fires after cooldown",    engine.check(calm, at(730)).type == TriggerType::Boredom);
}

// =============================================================================
// presence (webcam)
// =============================================================================
static void test_presence() {
    auto cfg = default_cfg();
    cfg.min_away_for_welcome_s = 180;
    cfg.away_leisure = false; // lazer testado à parte; aqui é só supressão/boas-vindas
    ProactivityEngine engine(cfg, t0());
    HormoneProfile calm;

    // Sem leitura de presença: comportamento normal (user_present = true)
    TEST("no reading -> assumed present",  engine.user_present());

    // Usuário ausente: nenhum gatilho fala com a cadeira vazia
    engine.note_presence(false, at(100));
    TEST("away suppresses boredom",        engine.check(calm, at(1000)).type == TriggerType::None);
    TEST("away state tracked",             !engine.user_present());

    // Voltou depois de 900s (> 180s): boas-vindas pendente
    engine.note_presence(true, at(1000));
    auto trig = engine.check(calm, at(1001));
    TEST("welcome fires on return",        trig.type == TriggerType::UserReturned);
    TEST("welcome mentions minutes",       trig.reason.find("15 minuto(s)") != std::string::npos);

    // Consumido: não repete
    TEST("welcome fires once",             engine.check(calm, at(1002)).type != TriggerType::UserReturned);

    // Ausência curta (< 180s) não gera boas-vindas
    ProactivityEngine engine2(cfg, t0());
    engine2.note_presence(true, at(10));
    engine2.note_presence(false, at(100));
    engine2.note_presence(true, at(200)); // fora por 100s < 180s
    TEST("short absence no welcome",       engine2.check(calm, at(201)).type != TriggerType::UserReturned);

    // Boas-vindas ignora o cooldown (voltar é evento, não spam)
    ProactivityEngine engine3(cfg, t0());
    engine3.note_proactive_message(at(50)); // cooldown ativo até 650
    engine3.note_presence(false, at(60));
    engine3.note_presence(true, at(400));
    TEST("welcome bypasses cooldown",      engine3.check(calm, at(401)).type == TriggerType::UserReturned);
}

// =============================================================================
// away leisure (Alyssa se distrai quando o usuário sai)
// =============================================================================
static void test_away_leisure() {
    auto cfg = default_cfg();
    cfg.leisure_after_away_s = 240;
    ProactivityEngine engine(cfg, t0());
    HormoneProfile calm;

    engine.note_presence(true, at(10));
    engine.note_presence(false, at(100)); // saiu

    // Ausência ainda curta: nada
    TEST("no leisure before threshold",    engine.check(calm, at(200)).type == TriggerType::None);

    // 240s+ fora: lazer dispara
    auto trig = engine.check(calm, at(350));
    TEST("leisure fires after threshold",  trig.type == TriggerType::AwayLeisure);
    TEST("leisure mentions open_url",      trig.reason.find("open_url") != std::string::npos);

    // Uma vez por ausência
    TEST("leisure fires once per absence", engine.check(calm, at(500)).type == TriggerType::None);

    // Voltou: boas-vindas menciona a distração
    engine.note_presence(true, at(600));
    trig = engine.check(calm, at(601));
    TEST("welcome after leisure",          trig.type == TriggerType::UserReturned);
    TEST("welcome mentions distraction",   trig.reason.find("se distraiu") != std::string::npos);

    // Nova ausência: lazer rearmado
    engine.note_presence(false, at(700));
    TEST("new absence rearms leisure",     engine.check(calm, at(1000)).type == TriggerType::AwayLeisure);

    // Desligado por config
    auto cfg2 = default_cfg();
    cfg2.away_leisure = false;
    ProactivityEngine engine2(cfg2, t0());
    engine2.note_presence(true, at(10));
    engine2.note_presence(false, at(100));
    TEST("leisure disabled by config",     engine2.check(calm, at(1000)).type == TriggerType::None);
}

// =============================================================================
// enabled flag
// =============================================================================
static void test_disabled() {
    auto cfg = default_cfg();
    cfg.enabled = false;
    ProactivityEngine engine(cfg, t0());
    HormoneProfile calm;
    TEST("disabled never fires",           engine.check(calm, at(100000)).type == TriggerType::None);
}

int main() {
    test_config_loading();
    test_boredom();
    test_cooldown();
    test_mood_triggers();
    test_startup_grace();
    test_presence();
    test_away_leisure();
    test_disabled();

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << tests_passed << "  Failed: " << tests_failed << "\n";
    std::cout << "========================================\n";
    return tests_failed == 0 ? 0 : 1;
}
