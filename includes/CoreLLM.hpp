
#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declarations (para não expor headers internos)
namespace internal {
    class SocialLLM;
    class EmotionLLM;
    class MemoryLLM;
    class ActionLLM;
    class IntrospectionLLM;
    class CentralMemory;
    class AlyssaLLM;
}

namespace alyssa_core {
    class AlyssaCore;
}

class CoreIntegration {
private:
    // Ponteiros inteligentes — tudo é privado
    std::unique_ptr<internal::SocialLLM> social;
    std::unique_ptr<internal::EmotionLLM> emotion;
    std::unique_ptr<internal::MemoryLLM> memory;
    std::unique_ptr<internal::ActionLLM> action;
    std::unique_ptr<internal::IntrospectionLLM> introspection;
    std::unique_ptr<internal::AlyssaLLM> alyssa;
    
    //TODO: Make this shit work
    //std::unique_ptr<internal::CentralMemory> central_memory;

    // Membro Ativc
    std::unique_ptr<alyssa_core::AlyssaCore> core_instance;

    std::string system_prompt;
    bool initialized = false;

public:
    CoreIntegration();
    ~CoreIntegration();

    bool initialize(const std::string& model_path);
    std::string think(const std::string& input);
    void act(const std::string& command);
    void reflect();
    void run_interactive_loop();
};
