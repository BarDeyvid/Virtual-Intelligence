#include "includes/httplib.h"
#include "includes/json.hpp"
#include "includes/CoreLLM.hpp"
#include <iostream>
#include <windows.h>
#include <filesystem>
#include <functional>
#include <memory> // Inclui std::unique_ptr e std::make_unique

namespace fs = std::filesystem;
using json = nlohmann::json;

// Supondo que a classe 'log' esteja definida em 'CoreLLM.hpp' ou similar.
// Se não estiver, esta linha pode causar erro, mas a assumimos para manter a lógica original do retry.
// Se o erro persistir, substitua log::error/log::warn por std::cerr/std::cout.

// MACRO CORRIGIDO: Agora aceita o ponteiro inteligente do core e o caminho do modelo.
#define HANDLE_WITH_RETRY(handler, core_ptr, model_path_ref) \
    [&core_ptr, &model_path_ref](const httplib::Request& req, httplib::Response& res) { \
        constexpr int MAX_ATTEMPTS = 3; \
        for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) { \
            try { \
                /* Chamamos o handler, que é a lógica real do endpoint */ \
                handler(req, res, core_ptr); \
                return; \
            } catch (const std::exception &e) { \
                /* Log de erro usando std::cerr se 'log' não estiver disponível */ \
                std::cerr << "Handler exception: " << e.what() << std::endl; \
                if (attempt == MAX_ATTEMPTS) { \
                    /* No último try, apenas falha e retorna erro 500 */ \
                    res.status = 500; \
                    json error = {{"success", false}, {"error", "Server failed after multiple retries: " + std::string(e.what())}}; \
                    res.set_content(error.dump(), "application/json"); \
                    return; \
                } \
                std::cout << "Restarting CoreIntegration – attempt " << (attempt + 1) \
                          << " with model: " << model_path_ref << std::endl; \
                \
                /* Recria e reinicializa o CoreIntegration usando o ponteiro inteligente */ \
                core_ptr = std::make_unique<CoreIntegration>(); \
                if (!core_ptr->initialize(model_path_ref)) { \
                    std::cerr << "CRITICAL ERROR: Failed to re-initialize model: " << model_path_ref << std::endl; \
                    res.status = 500; \
                    json error = {{"success", false}, {"error", "Critical initialization failure."}}; \
                    res.set_content(error.dump(), "application/json"); \
                    return; \
                } \
            } \
        } \
    }

// Definição da função de handler para /think e /think/fusion para ser reutilizada no macro.
// Ela agora recebe o core_ptr como argumento.
void think_handler_logic(const httplib::Request& req, httplib::Response& res, std::unique_ptr<CoreIntegration>& core_ptr) {
    auto body = json::parse(req.body);
    
    if (!body.contains("input")) {
        json error = {{"success", false}, {"error", "Missing 'input' field"}};
        res.set_content(error.dump(), "application/json");
        return;
    }
    
    std::string input = body["input"];
    std::string expert_id = body.value("expert_id", "");
    
    // O erro será capturado pelo macro HANDLE_WITH_RETRY
    std::string result = core_ptr->think_with_fusion_ttsless(input);
    
    json response = {
        {"success", true},
        {"output", result},
        {"expert_used", expert_id}
    };
    res.set_content(response.dump(), "application/json");
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::locale::global(std::locale("pt_BR.UTF-8"));
    httplib::Server svr;

    // 1. MUDANÇA: Use std::unique_ptr para que o macro possa recriar o objeto
    std::unique_ptr<CoreIntegration> core_ptr = std::make_unique<CoreIntegration>();

    // 2. MUDANÇA: Variável para rastrear o modelo carregado para fins de retry
    std::string previous_model_path = "models/gemma-3-1b-it-q4_0.gguf"; 

    if (fs::exists(previous_model_path)) {
        std::cout << "Inicializando modelo automaticamente: " << previous_model_path << std::endl;
        // Usa core_ptr->initialize
        if (!core_ptr->initialize(previous_model_path)) {
            std::cerr << "ERRO: Falha na inicialização automática do modelo!" << std::endl;
            // Se falhar na primeira vez, podemos ainda rodar o servidor, mas com o core vazio
            core_ptr.reset(); 
        } else {
             std::cout << "✅ Modelo inicializado com sucesso!" << std::endl;
        }
    } else {
        std::cout << "⚠️  Modelo não encontrado em: " << previous_model_path << std::endl;
        std::cout << "ℹ️  Use o endpoint /initialize para carregar um modelo" << std::endl;
    }


    // Health check
    svr.Get("/health", [&core_ptr](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"status", core_ptr ? "healthy" : "uninitialized"}, // Checa se o ponteiro é válido
            {"initialized", (bool)core_ptr}
        };
        res.set_content(response.dump(), "application/json");
    });
    
    // Think endpoint padrão: USA O MACRO CORRIGIDO
    svr.Post("/think", HANDLE_WITH_RETRY(think_handler_logic, core_ptr, previous_model_path));
    
    // 🆕 NOVO ENDPOINT: Think com fusão sem TTS: USA O MACRO CORRIGIDO
    // Note que a lógica interna do think_handler_logic não diferenciava /think de /think/fusion,
    // mas o macro garante o retry para ambos.
    svr.Post("/think/fusion", HANDLE_WITH_RETRY(think_handler_logic, core_ptr, previous_model_path));
    
    // Initialize endpoint
    svr.Post("/initialize", [&core_ptr, &previous_model_path](const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        
        if (!body.contains("base_model_path")) {
            json error = {{"success", false}, {"error", "Missing 'base_model_path' field"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        std::string new_model_path = body["base_model_path"];
        
        if (!fs::exists(new_model_path)) {
            json error = {
                {"success", false},
                {"error", "Model file not found: " + new_model_path}
            };
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        std::cout << "🔄 Inicializando modelo: " << new_model_path << std::endl;
        
        // Recria a instância
        core_ptr = std::make_unique<CoreIntegration>();
        bool success = core_ptr->initialize(new_model_path);
        
        if (success) {
            // ATUALIZA O PATH APÓS INICIALIZAÇÃO BEM-SUCEDIDA
            previous_model_path = new_model_path; 
        }

        json response = {
            {"success", success},
            {"message", success ? "Model initialized successfully" : "Initialization failed"},
            {"model_path", previous_model_path}
        };
        res.set_content(response.dump(), "application/json");
        
    } catch (const std::exception& e) {
        json error = {
            {"success", false},
            {"error", e.what()}
        };
        res.set_content(error.dump(), "application/json");
    }
});
    
    std::cout << "\nServer running on http://localhost:8181" << std::endl;
    svr.listen("0.0.0.0", 8181);
    
    return 0;
}