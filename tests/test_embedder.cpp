#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <string>
#include <tuple>
#include <stdexcept>
#include <curl/curl.h>
#include "includes/json.hpp"

using json = nlohmann::json;

// --- Funções de Similaridade de Cosseno ---
double dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return 0.0;
    double result = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double magnitude(const std::vector<float>& a) {
    double sum_of_squares = 0.0;
    for (float val : a) {
        sum_of_squares += val * val;
    }
    return std::sqrt(sum_of_squares);
}

double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    double dot = dot_product(a, b);
    double mag_a = magnitude(a);
    double mag_b = magnitude(b);
    if (mag_a == 0.0 || mag_b == 0.0) return 0.0;
    return dot / (mag_a * mag_b);
}

// --- Callback para escrever dados do cURL ---
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

// --- Função para extrair embedding de diferentes formatos JSON ---
// --- Função para extrair embedding de diferentes formatos JSON ---
std::vector<float> extract_embedding_from_json(const json& response_data) {
    std::vector<float> embedding;
    
    // Tentar diferentes formatos possíveis
    
    // Formato 1: Array direto com embeddings (formato simples)
    if (response_data.is_array() && !response_data.empty()) {
        // Verificar se é um array de números (embedding direto)
        if (response_data[0].is_number()) {
            for (const auto& value : response_data) {
                embedding.push_back(value.get<float>());
            }
            return embedding;
        }
        // Formato 2: Array de objetos com campo "embedding" (formato llama.cpp)
        else if (response_data[0].is_object() && response_data[0].contains("embedding")) {
            auto embedding_array = response_data[0]["embedding"];
            
            // Verificar se o embedding é um array 2D (formato aninhado)
            if (embedding_array.is_array() && !embedding_array.empty()) {
                // Se o primeiro elemento é um array, então é 2D
                if (embedding_array[0].is_array()) {
                    // Extrair do array 2D - pegar o primeiro array interno
                    for (const auto& value : embedding_array[0]) {
                        embedding.push_back(value.get<float>());
                    }
                } 
                // Se o primeiro elemento é número, então é 1D
                else if (embedding_array[0].is_number()) {
                    for (const auto& value : embedding_array) {
                        embedding.push_back(value.get<float>());
                    }
                }
            }
            return embedding;
        }
    }
    
    // Formato 3: Objeto com campo "embedding" no nível raiz
    if (response_data.is_object() && response_data.contains("embedding")) {
        auto embedding_array = response_data["embedding"];
        
        // Verificar se é array 2D
        if (embedding_array.is_array() && !embedding_array.empty() && embedding_array[0].is_array()) {
            for (const auto& value : embedding_array[0]) {
                embedding.push_back(value.get<float>());
            }
        } else {
            for (const auto& value : embedding_array) {
                embedding.push_back(value.get<float>());
            }
        }
        return embedding;
    }
    
    // Formato 4: Objeto com campo "data" contendo array de objetos com embedding
    if (response_data.is_object() && response_data.contains("data")) {
        auto data_array = response_data["data"];
        if (data_array.is_array() && !data_array.empty() && data_array[0].contains("embedding")) {
            auto embedding_array = data_array[0]["embedding"];
            
            // Verificar se é array 2D
            if (embedding_array.is_array() && !embedding_array.empty() && embedding_array[0].is_array()) {
                for (const auto& value : embedding_array[0]) {
                    embedding.push_back(value.get<float>());
                }
            } else {
                for (const auto& value : embedding_array) {
                    embedding.push_back(value.get<float>());
                }
            }
            return embedding;
        }
    }
    
    throw std::runtime_error("Formato JSON não reconhecido. Estrutura recebida: " + response_data.dump());
}

// --- Gerar embedding via API HTTP ---
std::vector<float> generate_embedding_via_api(const std::string& text) {
    CURL* curl;
    CURLcode res;
    std::string response;
    
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Falha ao inicializar cURL");
    }
    
    // Preparar o JSON da requisição
    json request_data;
    request_data["input"] = text;
    std::string json_data = request_data.dump();
    
    // Configurar a requisição cURL
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/embedding");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_data.length());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Timeout de 30 segundos
    
    // Configurar headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Configurar callback para resposta
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Executar a requisição
    res = curl_easy_perform(curl);
    
    // Limpar
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error("Falha na requisição HTTP: " + std::string(curl_easy_strerror(res)));
    }
    
    // Parse da resposta JSON
    try {
        json response_data = json::parse(response);
        std::cout << "Resposta JSON recebida: " << response_data.dump(2) << std::endl; // DEBUG
        
        return extract_embedding_from_json(response_data);
        
    } catch (const json::exception& e) {
        throw std::runtime_error("Erro ao parsear JSON: " + std::string(e.what()) + "\nResposta: " + response);
    }
}

// --- Função Principal ---
int main() {
    // Inicializar cURL globalmente
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    std::string query = "Which planet is known as the Red Planet?";
    std::vector<std::string> documents = {
        "Venus is often called Earth's twin because of its similar size and proximity.",
        "Mars, known for its reddish appearance, is often referred to as the Red Planet.",
        "Jupiter, the largest planet in our solar system, has a prominent red spot.",
        "Saturn, famous for its rings, is sometimes mistaken for the Red Planet."
    };

    std::cout << "Conectando ao servidor llama.cpp...\n";
    std::cout << "------------------------------------------------\n";

    try {
        // Testar conexão primeiro
        std::cout << "-> Testando conexão com o servidor...\n";
        auto test_emb = generate_embedding_via_api("test");
        std::cout << "Conexão bem-sucedida! Dimensão do embedding: " << test_emb.size() << "\n";
        
        // Gerar embeddings
        std::cout << "-> Gerando Embedding da Query...\n";
        std::vector<float> query_emb = generate_embedding_via_api(query);
        std::cout << "Query embedding - Dimensão: " << query_emb.size() << ", Magnitude: " << magnitude(query_emb) << "\n";
        
        std::vector<std::vector<float>> doc_embeddings;
        for (size_t i = 0; i < documents.size(); ++i) {
            std::cout << "-> Gerando Embedding do Documento " << i+1 << "...\n";
            auto emb = generate_embedding_via_api(documents[i]);
            doc_embeddings.push_back(emb);
            std::cout << "Doc " << i+1 << " - Dimensão: " << emb.size() << ", Magnitude: " << magnitude(emb) << "\n";
        }

        // Calcular similaridades
        std::cout << "\n-> Calculando Similaridade de Cosseno...\n";
        std::vector<std::tuple<double, int, std::string>> similarities;
        
        for (size_t i = 0; i < doc_embeddings.size(); ++i) {
            double sim = cosine_similarity(query_emb, doc_embeddings[i]);
            similarities.emplace_back(sim, i, documents[i]);
            std::cout << "Similaridade com documento " << i+1 << ": " << sim << "\n";
        }

        // Ordenar resultados
        std::sort(similarities.begin(), similarities.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) > std::get<0>(b);
        });

        // Exibir resultados
        std::cout << "\n------------------------------------------------\n";
        std::cout << "QUERY: " << query << "\n";
        std::cout << "------------------------------------------------\n";
        std::cout << "RANKING POR SIMILARIDADE:\n";
        
        for (const auto& item : similarities) {
            std::cout << std::fixed;
            std::cout.precision(4);
            std::cout << "[" << std::get<0>(item) << "] -> " << std::get<2>(item) << "\n";
        }
        std::cout << "------------------------------------------------\n";

    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << '\n';
        std::cerr << "Certifique-se de que:\n";
        std::cerr << "1. O servidor llama.cpp está rodando na porta 8080\n";
        std::cerr << "2. O endpoint /embedding está correto\n";
        std::cerr << "3. O modelo de embeddings está carregado\n";
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}