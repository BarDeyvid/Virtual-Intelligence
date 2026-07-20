// Bisecção do bug "sopa de token" do utility 1B (v2/F3).
// Roda de build/Release (precisa de models/ e DLLs).
//
// Variantes:
//   A) generate_raw de produção (min_p→temp→penalties→dist), rp=1.05
//   B) generate_raw de produção, rp=1.0 (penalidade desligada)
//   C) loop local com sampler GREEDY puro sobre o MESMO decode
// A limpa → resolvido. C sopa → o bug é decode/tokenização, não sampling.
#include "AlyssaCore.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static const char* INSTRUCTION =
    "Resuma em no máximo 2 frases: hoje o Deyvid começou a treinar boxe "
    "(aulas 19h terça e quinta) e tem entrega do projeto da faculdade na "
    "sexta, está atrasado.";

static std::string templated() {
    return std::string("<start_of_turn>user\n") + INSTRUCTION +
           "<end_of_turn>\n<start_of_turn>model\n";
}

/// Loop de geração mínimo: mesmo tokenize+decode do generate_raw, sampler greedy.
static std::string greedy_generate(alyssa_core::AlyssaCore& core, const std::string& prompt, int max_tokens) {
    const llama_vocab* vocab = core.get_vocab();
    llama_context* ctx = core.get_context();

    std::vector<llama_token> tokens(prompt.size() + 1);
    int n = llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                           tokens.data(), tokens.size(), true, true);
    if (n < 0) return "[tokenize falhou]";
    tokens.resize(n);
    printf("  (prompt = %d tokens; primeiro token id=%d)\n", n, tokens.empty() ? -1 : tokens[0]);

    llama_batch batch = llama_batch_get_one(tokens.data(), n);
    if (llama_decode(ctx, batch) != 0) return "[decode falhou]";

    llama_sampler* greedy = llama_sampler_init_greedy();
    std::string out;
    for (int i = 0; i < max_tokens; ++i) {
        llama_token t = llama_sampler_sample(greedy, ctx, -1);
        if (llama_vocab_is_eog(vocab, t)) break;
        char buf[512];
        int m = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
        if (m < 0) break;
        out.append(buf, m);
        llama_token nt[1] = {t};
        batch = llama_batch_get_one(nt, 1);
        if (llama_decode(ctx, batch) != 0) break;
    }
    llama_sampler_free(greedy);
    return out;
}

int main() {
    alyssa_core::AlyssaCore core("models/gemma-3-1b-it-q4_0.gguf", 4096);

    SimpleModelParameters p;
    p.temperature = 0.3;
    p.top_p = 0.8;
    p.max_tokens = 80;
    p.timeout_ms = 30000;

    printf("\n=== A) generate_raw producao, rp=1.05 ===\n");
    p.repeat_penalty = 1.05;
    core.clear_kv();
    printf("%s\n", core.generate_raw(templated(), p, nullptr, nullptr).c_str());

    printf("\n=== B) generate_raw producao, rp=1.0 (sem penalidade) ===\n");
    p.repeat_penalty = 1.0;
    core.clear_kv();
    printf("%s\n", core.generate_raw(templated(), p, nullptr, nullptr).c_str());

    printf("\n=== C) greedy puro sobre o mesmo decode ===\n");
    core.clear_kv();
    printf("%s\n", greedy_generate(core, templated(), 80).c_str());

    // D) Corpus LONGO estilo consolidação (~8000 chars): o daemon vira sopa
    // com prompts grandes; aqui reproduzimos o tamanho real.
    printf("\n=== D) generate_raw producao, corpus ~8KB, rp=1.05 ===\n");
    std::string corpus;
    const char* linhas[] = {
        "[alegria] oi | Oi. To aqui, o que voce quer?",
        "[neutral] qual sua opiniao sobre cafe da manha? | Cafe de manha e a unica coisa que me faz funcionar bem.",
        "[tristeza] to meio cansado hoje, o dia foi pesado | Puxa, que chato. Dormiu mal? Pega leve.",
        "[alegria] hoje comecei a treinar boxe! aula as 19h, terca e quinta | Que legal! Boxe, hein? Aguenta firme.",
        "[medo] e o projeto da faculdade entrega sexta, to meio atrasado nele | A entrega da sexta, hein? Bora organizar.",
        "[neutral] amanha tenho entrevista de emprego as 14h, to nervoso | Tranquilo, te lembro. Boa sorte!",
    };
    while (corpus.size() < 7800) {
        corpus += linhas[(corpus.size() / 90) % 6];
        corpus += "\n";
    }
    std::string prompt_d =
        "<start_of_turn>user\nVocê é a Alyssa. Resuma o seu dia com o Deyvid "
        "abaixo em 3 a 5 frases, em primeira pessoa, preservando fatos, nomes "
        "e decisões importantes. Responda APENAS com o resumo.\n\n" + corpus +
        "<end_of_turn>\n<start_of_turn>model\n";
    p.repeat_penalty = 1.05;
    p.max_tokens = 160;
    core.clear_kv();
    printf("%s\n", core.generate_raw(prompt_d, p, nullptr, nullptr).c_str());

    // E) Mesmo tamanho, corpus em PROSA (sem colchetes) + instrução DEPOIS
    // do corpus: hipótese = o 1B imita o formato "[tag] a | b" e degenera;
    // instrução no fim segura modelos pequenos.
    printf("\n=== E) prosa + instrucao no fim, ~8KB, rp=1.05 ===\n");
    const char* prosa[] = {
        "Deyvid disse que estava cansado e a Alyssa mandou ele pegar leve.",
        "Deyvid comecou a treinar boxe, com aulas as 19h de terca e quinta.",
        "O projeto da faculdade dele entrega na sexta e ele esta atrasado.",
        "Deyvid tem uma entrevista de emprego amanha as 14h e esta nervoso.",
        "Eles conversaram sobre cafe da manha e acai com morango.",
        "A Alyssa prometeu lembrar Deyvid de contar como foi a entrevista.",
    };
    std::string corpus_e;
    while (corpus_e.size() < 7800) {
        corpus_e += prosa[(corpus_e.size() / 80) % 6];
        corpus_e += "\n";
    }
    std::string prompt_e =
        "<start_of_turn>user\nDIÁRIO DO DIA:\n" + corpus_e +
        "\nVocê é a Alyssa. Resuma o SEU dia com o Deyvid descrito no diário "
        "acima em 3 a 5 frases, em primeira pessoa, preservando fatos, nomes e "
        "decisões. Responda APENAS com o resumo.<end_of_turn>\n<start_of_turn>model\n";
    core.clear_kv();
    printf("%s\n", core.generate_raw(prompt_e, p, nullptr, nullptr).c_str());

    // F) Corpus com colchetes (formato original), mas instrução no FIM —
    // isola formato vs posição da instrução.
    printf("\n=== F) colchetes + instrucao no fim, ~8KB, rp=1.05 ===\n");
    std::string prompt_f =
        "<start_of_turn>user\nREGISTRO DO DIA:\n" + corpus +
        "\nVocê é a Alyssa. Resuma o seu dia com o Deyvid registrado acima em "
        "3 a 5 frases, em primeira pessoa. Responda APENAS com o resumo."
        "<end_of_turn>\n<start_of_turn>model\n";
    core.clear_kv();
    printf("%s\n", core.generate_raw(prompt_f, p, nullptr, nullptr).c_str());

    // G) O CORPUS REAL (dump do banco em corpus_real.txt) com o prompt
    // EXATO da run_consolidation — reprodução 1:1 do que o daemon faz.
    printf("\n=== G) corpus REAL do banco, prompt da producao ===\n");
    {
        std::ifstream f("corpus_real.txt");
        if (!f.is_open()) {
            printf("(corpus_real.txt ausente — pulado)\n");
        } else {
            std::stringstream ss;
            ss << f.rdbuf();
            std::string corpus_real = ss.str();
            printf("  (corpus: %zu chars)\n", corpus_real.size());
            std::string prompt_g =
                "<start_of_turn>user\n"
                "Você é a Alyssa. Resuma o seu dia com o Deyvid abaixo em 3 a 5 frases, "
                "em primeira pessoa, preservando fatos, nomes e decisões importantes. "
                "Responda APENAS com o resumo.\n\n" + corpus_real +
                "<end_of_turn>\n<start_of_turn>model\n";
            core.clear_kv();
            printf("%s\n", core.generate_raw(prompt_g, p, nullptr, nullptr).c_str());

            // H) MESMO conteúdo real, reformatado como DIÁLOGO natural:
            // "[emocao] input | resposta" → "Deyvid: input\nAlyssa: resposta".
            // Hipótese final: o 1B imita o formato de colchetes e degenera;
            // diálogo é o formato que ele viu no treino.
            printf("\n=== H) corpus real como dialogo, instrucao no fim ===\n");
            std::string dialogo;
            std::istringstream lines(corpus_real);
            std::string line;
            while (std::getline(lines, line)) {
                size_t close = line.find("] ");
                if (line.rfind("[", 0) == 0 && close != std::string::npos) {
                    line = line.substr(close + 2); // remove "[emocao] "
                }
                size_t sep = line.find(" | ");
                if (sep != std::string::npos) {
                    dialogo += "Deyvid: " + line.substr(0, sep) + "\n";
                    dialogo += "Alyssa: " + line.substr(sep + 3) + "\n";
                } else if (!line.empty()) {
                    dialogo += line + "\n";
                }
            }
            std::string prompt_h =
                "<start_of_turn>user\nCONVERSA DE ONTEM:\n" + dialogo +
                "\nVocê é a Alyssa. Resuma a conversa acima em 3 a 5 frases, em "
                "primeira pessoa (como Alyssa), preservando fatos, nomes e "
                "decisões importantes. Responda APENAS com o resumo."
                "<end_of_turn>\n<start_of_turn>model\n";
            core.clear_kv();
            printf("%s\n", core.generate_raw(prompt_h, p, nullptr, nullptr).c_str());
        }
    }

    return 0;
}
