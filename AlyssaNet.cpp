// CoreIntegration.cpp

#include "CoreLLM.hpp"
#include "AlyssaCore.hpp"
#include "includes/internal/SocialLLM.hpp"
#include "includes/internal/EmotionLLM.hpp"
#include "includes/internal/IntrospectionLLM.hpp"
#include "includes/internal/MemoryLLM.hpp"
#include "includes/internal/AlyssaLLM.hpp"
#include "includes/internal/ActionLLM.hpp" 

using namespace alyssa_core;
using namespace internal;

// =========================================================================
// Construtor & Destrutor (simples)
// =========================================================================

CoreIntegration::CoreIntegration() : initialized(false) {}

CoreIntegration::~CoreIntegration() {
}

bool CoreIntegration::initialize(const std::string& base_model_path) {
    if (initialized) {
        std::cerr << "AVISO: CoreIntegration já inicializado." << std::endl;
        return true;
    }

    try {
        // 1. Inicializa o Core com o modelo base
        core_instance = std::make_unique<AlyssaCore>(base_model_path);
        
        llama_model* shared_model = core_instance->get_model();
        const llama_vocab* shared_vocab = core_instance->get_vocab();

        // 2. Cria todos os ESPECIALISTAS
        social = std::make_unique<SocialLLM>(
            shared_model, shared_vocab, "socialModel"
        );
        emotion = std::make_unique<EmotionLLM>(
            shared_model, shared_vocab, "emotionalModel"
        );
        introspection = std::make_unique<IntrospectionLLM>(
            shared_model, shared_vocab, "introspectiveModel"
        );
        memory = std::make_unique<MemoryLLM>(
            shared_model, shared_vocab, "memoryModel"
        );
        alyssa = std::make_unique<AlyssaLLM>(
            shared_model, shared_vocab, "alyssa"
        );
        
        // 3. (OPCIONAL) Inicializa a memória central aqui (se você a implementar)
        // central_memory = std::make_unique<CentralMemory>();

        initialized = true;
        std::cout << "CoreIntegration inicializado com sucesso!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "ERRO CRÍTICO na Inicialização da CoreIntegration: %s\n", e.what());
        core_instance = nullptr; // Garantir que o core seja liberado em caso de falha
        return false;
    }
}

// =========================================================================
// 🧠 Método Think (Onde a lógica STT/TTS irá interagir)
// Este é o método principal que irá orquestrar todos os especialistas.
// Por padrão, vamos apenas usar o especialista principal (AlyssaLLM) ou o SocialLLM.
// =========================================================================

std::string CoreIntegration::think(const std::string& input) {
    if (!initialized || !social) {
        return "Erro: O CoreIntegration não foi inicializado corretamente.";
    }
    
    // 1. Processamento Emocional/Introspectivo (chamada silenciosa)
    // NOTE: Seus especialistas imprimem a resposta no console. Isso pode não ser
    // ideal para uma API de fundo (backend). Você pode querer modificar
    // generate_response para não imprimir.
    // std::string emotion_analysis = emotion->generate_response(input);
    // std::string introspection_data = introspection->generate_response(input);
    
    // 2. Memória (busca e update)
    // std::string context_from_memory = memory->retrieve_context(input);
    
    // 3. Geração da Resposta Final (usando o especialista Alyssa)
    // Para simplificar, vamos usar o especialista social ou o Alyssa como a saída final.
    
    printf("\033[33m[ALYSSA FINAL]: \033[0m"); 
    std::string final_response = alyssa->generate_response(input);
    printf("\n\033[0m");

    // 4. Ação (se necessário)
    // if (alyssa->decidiu_agir(final_response)) { act("comando"); }

    return final_response;
}

// Métodos act e reflect podem ser deixados vazios ou implementados
void CoreIntegration::act(const std::string& command) {
    // Lógica para executar ações (e.g., controlar um robô, chamar uma API)
    std::cout << "[ACTION]: Comando recebido: " << command << std::endl;
}

void CoreIntegration::reflect() {
    // Lógica para a reflexão interna e atualização de memória
    std::cout << "[REFLECTION]: Iniciando ciclo de reflexão..." << std::endl;
}

void CoreIntegration::run_interactive_loop() {
    // Este método é útil para testes de terminal, mas a API externa chamará
    // diretamente 'think'.
    std::cout << "Loop Interativo iniciado. Digite 'sair' para encerrar.\n";
    std::string user_input;

    while (true) {
        printf("\033[32m> \033[0m"); 
        std::getline(std::cin, user_input);
        if (user_input == "sair" || user_input.empty()) break;
        
        think(user_input);
    }
}