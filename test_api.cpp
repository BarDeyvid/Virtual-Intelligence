#include "includes/httplib.h"
#include "includes/json.hpp"
#include "includes/CoreLLM.hpp"
#include <iostream>
#include <windows.h>

using json = nlohmann::json;

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::locale::global(std::locale("pt_BR.UTF-8"));
    httplib::Server svr;
    CoreIntegration core;

    std::string model_path = "models/gemma-3-1b-it-q4_0.gguf"; 

    if (fs::exists(model_path)) {
        std::cout << "Inicializando modelo automaticamente: " << model_path << std::endl;
        if (!core.initialize(model_path)) {
            std::cerr << "ERRO: Falha na inicialização automática do modelo!" << std::endl;
            return -1;
        }
        std::cout << "✅ Modelo inicializado com sucesso!" << std::endl;
    } else {
        std::cout << "⚠️  Modelo não encontrado em: " << model_path << std::endl;
        std::cout << "ℹ️  Use o endpoint /initialize para carregar um modelo" << std::endl;
    }


    // Health check
    svr.Get("/health", [&core](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"status", "healthy"},
            {"initialized", true}
        };
        res.set_content(response.dump(), "application/json");
    });
    
    // Think endpoint padrão
    svr.Post("/think", [&core](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("input")) {
                json error = {{"success", false}, {"error", "Missing 'input' field"}};
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            std::string input = body["input"];
            std::string expert_id = body.value("expert_id", "");
            
            std::string result;
            
            result = core.think_with_fusion_ttsless(input);
            
            json response = {
                {"success", true},
                {"output", result},
                {"expert_used", expert_id}
            };
            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception& e) {
            json error = {
                {"success", false},
                {"error", e.what()}
            };
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // 🆕 NOVO ENDPOINT: Think com fusão sem TTS
    svr.Post("/think/fusion", [&core](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("input")) {
                json error = {{"success", false}, {"error", "Missing 'input' field"}};
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            std::string input = body["input"];
            
            // Usa o novo método sem TTS
            std::string result = core.think_with_fusion_ttsless(input);
            
            json response = {
                {"success", true},
                {"output", result},
                {"method", "weighted_fusion_ttsless"}
            };
            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception& e) {
            json error = {
                {"success", false},
                {"error", e.what()}
            };
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Initialize endpoint
    svr.Post("/initialize", [&core](const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        
        if (!body.contains("base_model_path")) {
            json error = {{"success", false}, {"error", "Missing 'base_model_path' field"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        std::string model_path = body["base_model_path"];
        
        // Verifica se o arquivo existe
        if (!fs::exists(model_path)) {
            json error = {
                {"success", false},
                {"error", "Model file not found: " + model_path}
            };
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        std::cout << "🔄 Inicializando modelo: " << model_path << std::endl;
        bool success = core.initialize(model_path);
        
        json response = {
            {"success", success},
            {"message", success ? "Model initialized successfully" : "Initialization failed"},
            {"model_path", model_path}
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
    
    std::cout << "Server running on http://localhost:8181" << std::endl;
    svr.listen("0.0.0.0", 8181);
    
    return 0;
}