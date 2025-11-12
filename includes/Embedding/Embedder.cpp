#include "Embedding/Embedder.hpp"

// Construtores e Destrutores
Embedder::Embedder() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Embedder::Embedder(const std::string& config_path) : config_path(config_path) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Embedder::~Embedder() {
    stop_llama_server();
    curl_global_cleanup();
}

// Inicialização
bool Embedder::initialize() {
    return initialize(config_path);
}

bool Embedder::initialize(const std::string& config_path_) {
    this->config_path = config_path_;
    
    // Criar diretório config se não existir
    fs::path config_dir = fs::path(config_path_).parent_path();
    if (!config_dir.empty() && !fs::exists(config_dir)) {
        fs::create_directories(config_dir);
    }
    
    // Carregar configuração
    if (!load_config(config_path_)) {
        std::cerr << "Falha ao carregar configuração. Criando configuração padrão..." << std::endl;
        create_default_config();
        if (!load_config(config_path_)) {
            std::cerr << "Erro: Não foi possível carregar ou criar configuração." << std::endl;
            return false;
        }
    }
    
    // Iniciar servidor se configurado
    if (config.auto_start_server) {
        if (!start_llama_server()) {
            std::cerr << "Falha ao iniciar servidor llama.cpp" << std::endl;
            return false;
        }
    }
    
    // Testar conexão
    try {
        std::cout << "Testando conexão com servidor de embeddings..." << std::endl;
        auto test_embedding = generate_embedding_impl("test");
        std::cout << "Conexão bem-sucedida! Dimensão do embedding: " << test_embedding.size() << std::endl;
        initialized = true;
    } catch (const std::exception& e) {
        std::cerr << "Falha na conexão com servidor: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

// Geração de embeddings
std::vector<float> Embedder::generate_embedding(const std::string& text) {
    if (!initialized) {
        throw std::runtime_error("Embedder não inicializado. Chame initialize() primeiro.");
    }
    return generate_embedding_impl(text);
}

std::vector<std::vector<float>> Embedder::generate_embeddings(const std::vector<std::string>& texts) {
    std::vector<std::vector<float>> embeddings;
    embeddings.reserve(texts.size());
    
    for (const auto& text : texts) {
        embeddings.push_back(generate_embedding(text));
    }
    
    return embeddings;
}

// Busca semântica
std::vector<std::tuple<double, int, std::string>> Embedder::semantic_search(
    const std::string& query, 
    const std::vector<std::string>& documents,
    int top_k) {
    
    if (!initialized) {
        throw std::runtime_error("Embedder não inicializado. Chame initialize() primeiro.");
    }
    
    std::cout << "Gerando embedding da query..." << std::endl;
    auto query_embedding = generate_embedding(query);
    
    std::cout << "Gerando embeddings dos documentos..." << std::endl;
    auto doc_embeddings = generate_embeddings(documents);
    
    std::vector<std::tuple<double, int, std::string>> similarities;
    for (size_t i = 0; i < documents.size(); ++i) {
        double sim = cosine_similarity(query_embedding, doc_embeddings[i]);
        similarities.emplace_back(sim, i, documents[i]);
    }
    
    // Ordenar por similaridade (maior primeiro)
    std::sort(similarities.begin(), similarities.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a) > std::get<0>(b);
    });
    
    // Retornar apenas top_k resultados
    if (top_k > 0 && top_k < static_cast<int>(similarities.size())) {
        similarities.resize(top_k);
    }
    
    return similarities;
}

// Implementação interna de geração de embedding
std::vector<float> Embedder::generate_embedding_impl(const std::string& text) {
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
    std::string url = get_server_url();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_data.length());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.request_timeout);
    
    // Configurar headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Configurar callback para resposta
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
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
        return extract_embedding_from_json(response_data);
    } catch (const json::exception& e) {
        throw std::runtime_error("Erro ao parsear JSON: " + std::string(e.what()) + "\nResposta: " + response);
    }
}

// Funções de similaridade (estáticas)
double Embedder::dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return 0.0;
    double result = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double Embedder::magnitude(const std::vector<float>& a) {
    double sum_of_squares = 0.0;
    for (float val : a) {
        sum_of_squares += val * val;
    }
    return std::sqrt(sum_of_squares);
}

double Embedder::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    double dot = dot_product(a, b);
    double mag_a = magnitude(a);
    double mag_b = magnitude(b);
    if (mag_a == 0.0 || mag_b == 0.0) return 0.0;
    return dot / (mag_a * mag_b);
}

// Callback do cURL
size_t Embedder::write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

// Extração de embedding do JSON
std::vector<float> Embedder::extract_embedding_from_json(const json& response_data) {
    std::vector<float> embedding;
    
    if (response_data.is_array() && !response_data.empty()) {
        if (response_data[0].is_number()) {
            for (const auto& value : response_data) {
                embedding.push_back(value.get<float>());
            }
            return embedding;
        }
        else if (response_data[0].is_object() && response_data[0].contains("embedding")) {
            auto embedding_array = response_data[0]["embedding"];
            
            if (embedding_array.is_array() && !embedding_array.empty()) {
                if (embedding_array[0].is_array()) {
                    for (const auto& value : embedding_array[0]) {
                        embedding.push_back(value.get<float>());
                    }
                } 
                else if (embedding_array[0].is_number()) {
                    for (const auto& value : embedding_array) {
                        embedding.push_back(value.get<float>());
                    }
                }
            }
            return embedding;
        }
    }
    
    if (response_data.is_object() && response_data.contains("embedding")) {
        auto embedding_array = response_data["embedding"];
        
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
    
    throw std::runtime_error("Formato JSON não reconhecido");
}

// Gerenciamento de configuração
bool Embedder::load_config(const std::string& config_path) {
    try {
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            return false;
        }
        
        json config_json;
        config_file >> config_json;
        
        if (config_json.contains("host")) config.host = config_json["host"];
        if (config_json.contains("port")) config.port = config_json["port"];
        if (config_json.contains("endpoint")) config.endpoint = config_json["endpoint"];
        if (config_json.contains("model_path")) config.model_path = config_json["model_path"];
        if (config_json.contains("server_executable")) config.server_executable = config_json["server_executable"];
        if (config_json.contains("auto_start_server")) config.auto_start_server = config_json["auto_start_server"];
        if (config_json.contains("server_timeout")) config.server_timeout = config_json["server_timeout"];
        if (config_json.contains("request_timeout")) config.request_timeout = config_json["request_timeout"];
        if (config_json.contains("embedding_dimension")) config.embedding_dimension = config_json["embedding_dimension"];
        
        return true;
    } catch (...) {
        return false;
    }
}

bool Embedder::save_config(const std::string& config_path) {
    try {
        json config_json;
        config_json["host"] = config.host;
        config_json["port"] = config.port;
        config_json["endpoint"] = config.endpoint;
        config_json["model_path"] = config.model_path;
        config_json["server_executable"] = config.server_executable;
        config_json["auto_start_server"] = config.auto_start_server;
        config_json["server_timeout"] = config.server_timeout;
        config_json["request_timeout"] = config.request_timeout;
        config_json["embedding_dimension"] = config.embedding_dimension;
        
        std::ofstream config_file(config_path);
        config_file << config_json.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

void Embedder::create_default_config() {
    config = Config();
    save_config(config_path);
}

// Gerenciamento do servidor
bool Embedder::start_llama_server() {
    if (config.model_path.empty()) {
        std::cerr << "Erro: model_path não especificado na configuração" << std::endl;
        return false;
    }
    
    if (!fs::exists(config.server_executable)) {
        std::cerr << "Erro: Servidor não encontrado em: " << config.server_executable << std::endl;
        return false;
    }
    
    if (!fs::exists(config.model_path)) {
        std::cerr << "Erro: Modelo não encontrado em: " << config.model_path << std::endl;
        return false;
    }
    
    std::cout << "Iniciando servidor llama.cpp..." << std::endl;
    server_thread = std::make_unique<std::thread>(&Embedder::server_process, this);
    
    // Aguardar servidor ficar pronto
    if (!wait_for_server_ready(config.server_timeout)) {
        std::cerr << "Servidor não ficou pronto dentro do timeout" << std::endl;
        stop_llama_server();
        return false;
    }
    
    server_started = true;
    std::cout << "Servidor iniciado com sucesso!" << std::endl;
    return true;
}

void Embedder::stop_llama_server() {
    if (server_thread && server_thread->joinable()) {
        // Em um sistema real, enviaríamos um sinal para o processo
        server_thread->detach(); // Não é ideal, mas simples para demonstração
    }
    server_started = false;
}

void Embedder::server_process() {
    std::string command = config.server_executable + 
                         " -m " + config.model_path + 
                         " --port " + std::to_string(config.port) + 
                         " --embedding";
    
    std::cout << "Executando: " << command << std::endl;
    int result = std::system(command.c_str());
    
    if (result != 0) {
        std::cerr << "Servidor terminou com código: " << result << std::endl;
    }
}

bool Embedder::wait_for_server_ready(int timeout_seconds) {
    auto start = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_seconds)) {
        try {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string url = "http://" + config.host + ":" + std::to_string(config.port);
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
                
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                
                if (res == CURLE_OK) {
                    return true;
                }
            }
        } catch (...) {
            // Ignorar erros e continuar tentando
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return false;
}