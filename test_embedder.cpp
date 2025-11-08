#include "Embedding/Embedder.hpp"
#include <iostream>

int main() {
    try {
        // Inicializar embedder com configuração
        Embedder embedder;
        if (!embedder.initialize("config/embedder_config.json")) {
            std::cerr << "Falha ao inicializar Embedder" << std::endl;
            return 1;
        }
        
        std::string query = "Which planet is known as the Red Planet?";
        std::vector<std::string> documents = {
            "Venus is often called Earth's twin because of its similar size and proximity.",
            "Mars, known for its reddish appearance, is often referred to as the Red Planet.",
            "Jupiter, the largest planet in our solar system, has a prominent red spot.",
            "Saturn, famous for its rings, is sometimes mistaken for the Red Planet."
        };
        
        std::cout << "Realizando busca semântica..." << std::endl;
        auto results = embedder.semantic_search(query, documents, 3);
        
        std::cout << "\n------------------------------------------------\n";
        std::cout << "QUERY: " << query << "\n";
        std::cout << "------------------------------------------------\n";
        std::cout << "TOP 3 RESULTADOS:\n";
        
        for (const auto& [score, index, text] : results) {
            std::cout << std::fixed;
            std::cout.precision(4);
            std::cout << "[" << score << "] -> " << text << "\n\n";
        }
        std::cout << "------------------------------------------------\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}