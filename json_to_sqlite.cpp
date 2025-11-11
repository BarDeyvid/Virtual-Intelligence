#include <AlyssaMemoryHandler.hpp> 
#include "Embedding/Embedder.hpp"            
#include <filesystem>
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <memory>
#include <ctime>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <regex>
#include <numeric>
#include "includes/json.hpp" // Biblioteca nlohmann::json

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==========================================================
// FUNÇÃO AJUSTADA PARA RETORNAR O VETOR DE INTERAÇÕES
// ==========================================================
std::vector<std::pair<std::string, std::string>> process_json_file(const std::string& filename) {
    std::ifstream file(filename);
    // Cria o vetor que irá armazenar e retornar as interações
    std::vector<std::pair<std::string, std::string>> interactions;

    if (!file.is_open()) {
        std::cerr << "Erro: Não foi possível abrir o arquivo JSON: " << filename << std::endl;
        return interactions; 
    }
    
    json data;
    try {
        file >> data; // Faz o parse do JSON do arquivo
    } catch (const json::parse_error& e) {
        std::cerr << "Erro ao fazer o parse do JSON: " << e.what() << std::endl;
        return interactions; 
    }

    if (!data.is_array()) {
        std::cerr << "Erro: O JSON não é um array de objetos. Retornando vetor vazio." << std::endl;
        return interactions; 
    }

    // Itera sobre cada objeto (conversa) no array JSON
    for (const auto& conversation : data) {
        try {
            // Acessa e converte as chaves "user_input" e "ai_response"
            std::string user_input = conversation.at("user_input").get<std::string>();
            std::string ai_response = conversation.at("ai_response").get<std::string>();
            
            // Adiciona o par (user_input, ai_response) ao vetor
            interactions.push_back({user_input, ai_response});
            
        } catch (const json::out_of_range& e) {
            std::cerr << "Aviso: Objeto no JSON sem 'user_input' ou 'ai_response'. Item ignorado." << std::endl;
        } catch (const json::type_error& e) {
            std::cerr << "Aviso: Tipo de dado incorreto para 'user_input' ou 'ai_response'. Item ignorado." << std::endl;
        }
    }
    
    std::cout << "\nSucesso! " << interactions.size() << " interações carregadas de " << filename << std::endl;
    return interactions;
}

// ==========================================================
// FUNÇÃO MAIN COM A CHAMADA CORRETA
// ==========================================================
int main() {
    try {
        AlyssaMemoryManager memory_manager("alyssa_advanced_memory.db", true);
        
        // 1. CHAMA A FUNÇÃO E ARMAZENA O RESULTADO DO JSON
        std::vector<std::pair<std::string, std::string>> fairy_interactions = 
            process_json_file("fairy_conversations_v2.json");
        
        // 2. Itera sobre o vetor populado com dados do JSON
        for (size_t i = 0; i < fairy_interactions.size(); ++i) {
            const std::string& user_input = fairy_interactions[i].first;
            const std::string& ai_response = fairy_interactions[i].second;

            std::cout << "\n=== Interação " << (i + 1) << " ===\n";
            std::cout << "Usuário: " << user_input << "\n";
            std::cout << "Alyssa: " << ai_response << "\n";
            
            // 3. Processa a interação com o Memory Manager
            memory_manager.processInteraction(
                user_input, 
                ai_response
            );
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Erro fatal na execução: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}