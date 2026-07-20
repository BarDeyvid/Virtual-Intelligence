// Testes do SelfState (v2/F2) — header-only, sem modelo, sem GPU.
// Roda de qualquer diretório: usa um path temporário próprio.
#include "SelfState.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

static int passed = 0;
static int failed = 0;

#define CHECK(name, cond)                                   \
    do {                                                    \
        if (cond) { ++passed; }                             \
        else { ++failed; printf("FALHOU: %s\n", name); }    \
    } while (0)

static const char* TEST_FILE = "test_self_state_tmp/self.json";

static void test_first_life() {
    std::filesystem::remove_all("test_self_state_tmp");
    auto s = alyssa_self::load_self(TEST_FILE);
    CHECK("primeira vida: loaded=false", !s.loaded);
    CHECK("primeira vida: bloco [EU] vazio", alyssa_self::render_self_block(s).empty());
}

static void test_roundtrip() {
    alyssa_self::SelfState s;
    alyssa_self::upsert_opinion(s, "café", "essencial de manhã", 0.8);
    alyssa_self::update_goal(s, "ficar boa de Minecraft", "aprendeu a craftar", 0.7);
    s.inside_jokes.push_back("colega de quarto que não paga aluguel");
    alyssa_self::add_agenda(s, "perguntar do projeto X", "ele comentou ontem", 3);
    s.hormones = {{"cortisol", 0.6}, {"dopamine", 0.9}};
    s.last_daily_seed = 20260719;

    CHECK("save ok", alyssa_self::save_self(s, TEST_FILE));

    auto r = alyssa_self::load_self(TEST_FILE);
    CHECK("roundtrip: loaded", r.loaded);
    CHECK("roundtrip: opinião", r.opinions.size() == 1 && r.opinions[0].topic == "café"
                                && r.opinions[0].confidence > 0.79);
    CHECK("roundtrip: meta", r.goals.size() == 1 && r.goals[0].priority > 0.69);
    CHECK("roundtrip: piada", r.inside_jokes.size() == 1);
    CHECK("roundtrip: agenda", r.agenda.size() == 1 && r.agenda[0].expires_at > 0);
    CHECK("roundtrip: hormônios", r.hormones.at("dopamine") > 0.89);
    CHECK("roundtrip: daily seed", r.last_daily_seed == 20260719);
    CHECK("roundtrip: saved_at", r.saved_at > 0);
}

static void test_upsert_dedup() {
    alyssa_self::SelfState s;
    alyssa_self::upsert_opinion(s, "café", "bom", 0.5);
    alyssa_self::upsert_opinion(s, "café", "essencial", 0.9);
    CHECK("upsert: não duplica topic", s.opinions.size() == 1);
    CHECK("upsert: stance atualizada", s.opinions[0].stance == "essencial");

    alyssa_self::update_goal(s, "meta A", "início");
    alyssa_self::update_goal(s, "meta A", "meio", 0.9);
    CHECK("goal: não duplica desc", s.goals.size() == 1);
    CHECK("goal: progresso atualizado", s.goals[0].progress == "meio" && s.goals[0].priority > 0.89);
}

static void test_offline_decay() {
    std::map<std::string, double> saved = {{"cortisol", 0.8}, {"dopamine", 0.5}};
    std::map<std::string, double> out;
    auto set = [&](const std::string& n, double v) { out[n] = v; };

    // 0h: nível preservado
    alyssa_self::apply_offline_decay(saved, 0.0, set);
    CHECK("decay 0h: preserva", std::abs(out["cortisol"] - 0.8) < 1e-9);

    // TAU horas: baseline + desvio * e^-1 → 0.2 + 0.6*0.3679 ≈ 0.4207
    alyssa_self::apply_offline_decay(saved, alyssa_self::OFFLINE_DECAY_TAU_HOURS, set);
    CHECK("decay TAU: ~37% do desvio", std::abs(out["cortisol"] - 0.4207) < 0.01);

    // Dopamina JÁ na baseline (0.5): não se move nunca
    CHECK("decay: baseline não se move", std::abs(out["dopamine"] - 0.5) < 1e-9);

    // 30 dias: praticamente baseline
    alyssa_self::apply_offline_decay(saved, 720.0, set);
    CHECK("decay 30d: baseline", std::abs(out["cortisol"] - 0.2) < 0.001);
}

static void test_agenda_prune() {
    alyssa_self::SelfState s;
    alyssa_self::add_agenda(s, "vence amanhã", "", 1);
    alyssa_self::add_agenda(s, "sem validade", "", 0);
    long long future = alyssa_self::now_epoch() + 86400LL * 10;
    alyssa_self::prune_agenda(s, future);
    CHECK("prune: expira vencidos, mantém sem validade",
          s.agenda.size() == 1 && s.agenda[0].bring_up == "sem validade");
}

static void test_agenda_dedup() {
    alyssa_self::SelfState s;
    alyssa_self::add_agenda(s, "perguntar da entrevista", "nervoso", 3);
    alyssa_self::add_agenda(s, "perguntar da entrevista", "", 5);
    CHECK("agenda: add repetido só renova", s.agenda.size() == 1);

    // Self-healing: duplicata pré-existente (self.json antigo) morre no prune
    alyssa_self::AgendaItem dup;
    dup.bring_up = "perguntar da entrevista";
    dup.expires_at = alyssa_self::now_epoch() + 86400;
    s.agenda.push_back(dup);
    alyssa_self::prune_agenda(s);
    CHECK("agenda: prune remove duplicata", s.agenda.size() == 1);
}

static void test_render_caps() {
    alyssa_self::SelfState s;
    for (int i = 0; i < 8; ++i) {
        alyssa_self::upsert_opinion(s, "topico" + std::to_string(i), "stance", 0.5);
    }
    std::string block = alyssa_self::render_self_block(s);
    CHECK("render: tem [EU]", block.find("[EU]") != std::string::npos
                              && block.find("[/EU]") != std::string::npos);
    // Cap de 5 opiniões: conta os "- " de opinião
    size_t count = 0, pos = 0;
    while ((pos = block.find("- topico", pos)) != std::string::npos) { ++count; ++pos; }
    CHECK("render: cap de 5 opiniões", count == 5);
}

static void test_yesterday_roundtrip() {
    alyssa_self::SelfState s;
    s.yesterday_summary = "Ontem falamos de açaí e da entrevista.";
    s.yesterday_date = 20260719;
    s.last_consolidation_date = 20260720;
    alyssa_self::save_self(s, TEST_FILE);

    auto r = alyssa_self::load_self(TEST_FILE);
    CHECK("ontem: resumo persiste", r.yesterday_summary == s.yesterday_summary);
    CHECK("ontem: datas persistem",
          r.yesterday_date == 20260719 && r.last_consolidation_date == 20260720);
    CHECK("ontem: bloco renderiza",
          alyssa_self::render_yesterday_block(r).find("açaí") != std::string::npos);

    alyssa_self::SelfState vazio;
    CHECK("ontem: sem resumo → sem bloco",
          alyssa_self::render_yesterday_block(vazio).empty());
}

static void test_opinion_drift() {
    alyssa_self::SelfState s;
    long long now = alyssa_self::now_epoch();

    alyssa_self::upsert_opinion(s, "fresca", "reforçada hoje", 0.5);
    alyssa_self::upsert_opinion(s, "velha", "sem reforço", 0.5);
    s.opinions[1].last_reinforced = now - 20LL * 86400;  // 20 dias parada
    alyssa_self::upsert_opinion(s, "moribunda", "quase morta", 0.205);
    s.opinions[2].last_reinforced = now - 20LL * 86400;

    int dropped = alyssa_self::drift_opinions(s, now);
    CHECK("drift: fresca intacta", s.opinions[0].confidence > 0.499);
    CHECK("drift: velha decai 5%", std::abs(s.opinions[1].confidence - 0.475) < 1e-6);
    // 0.205 * 0.95 = 0.19475 < 0.2 → desapega
    CHECK("drift: moribunda morre", dropped == 1 && s.opinions.size() == 2);
}

int main() {
    test_first_life();
    test_roundtrip();
    test_upsert_dedup();
    test_offline_decay();
    test_agenda_prune();
    test_agenda_dedup();
    test_render_caps();
    test_yesterday_roundtrip();
    test_opinion_drift();

    std::filesystem::remove_all("test_self_state_tmp");

    printf("\n=== Results ===\nPassed: %d\nFailed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
