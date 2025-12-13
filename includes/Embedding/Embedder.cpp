// Embedder.cpp
#include "Embedding/Embedder.hpp"


// Construtores e Destrutores
Embedder::Embedder() {
    llama_backend_init();
}

Embedder::Embedder(const std::string& config_path) : config_path(config_path) {
    llama_backend_init();
}

Embedder::~Embedder() {
    if (initialized) {
        llama_batch_free(batch);
        context.reset();
        model.reset();
    }
    llama_backend_free();
}

bool Embedder::initialize() {
    return initialize(config_path);
}


bool Embedder::initialize(const std::string& config_path_) {
    llama_log_set([](enum ggml_log_level level, const char * text, void * /* user_data */) {
        if (level >= GGML_LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);

    this->config_path = config_path_;
    
    // Gerenciamento de Config
    fs::path config_dir = fs::path(config_path_).parent_path();
    if (!config_dir.empty() && !fs::exists(config_dir)) {
        fs::create_directories(config_dir);
    }
    
    if (!config_load(config_path_)) {
        std::cerr << "Falha ao carregar configuração. Criando configuração padrão..." << std::endl;
        create_default_config();
        if (!config_load(config_path_)) {
            std::cerr << "Erro: Não foi possível carregar ou criar configuração." << std::endl;
            return false;
        }
    }

    // 1. Configurar parâmetros do MODELO
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = config.n_gpu_layers;
    mparams.use_mlock    = config.use_mlock;
    mparams.use_mmap     = config.use_mmap;

    // 2. Carregar o MODELO
    llama_model * model_ptr = llama_model_load_from_file(config.model_path.c_str(), mparams);
    if (model_ptr == nullptr) {
        std::cerr << __func__ << ": unable to load model from " << config.model_path << std::endl;
        return false;
    }
    model.reset(model_ptr, llama_model_free);

    // 3. Obter o vocabulário do modelo
    vocab_ptr = llama_model_get_vocab(model.get());
    
    // 4. Configurar parâmetros do CONTEXTO PARA EMBEDDING
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx       = 8192;  // Smaller context for embeddings
    cparams.n_batch     = 8192;  // Batch size for embeddings
    cparams.n_threads   = (config.n_threads <= 0) ? std::thread::hardware_concurrency() : config.n_threads;
    cparams.embeddings  = true;
    
    // 5. Criar o CONTEXTO DE EMBEDDING (separado do contexto de geração)
    llama_context * context_ptr = llama_init_from_model(model.get(), cparams);
    if (context_ptr == nullptr) {
        std::cerr << __func__ << ": failed to create embedding context from model" << std::endl;
        return false;
    }
    context.reset(context_ptr, llama_free);

    // 6. Obter informações do modelo
    pooling_type = llama_pooling_type(context.get());
    
    if (pooling_type == LLAMA_POOLING_TYPE_NONE) {
        std::cerr << __func__ << ": Error: Model must support sequence-level pooling (e.g., CLS or MEAN pooling)." << std::endl;
        return false;
    }
    
    if (llama_model_has_encoder(model.get()) && llama_model_has_decoder(model.get())) {
        std::cerr << __func__ << ": computing embeddings in encoder-decoder models is not supported" << std::endl;
        return false;
    }

    n_embd = llama_model_n_embd(model.get());
    config.embedding_dimension = n_embd;

    // 7. Inicializar batch
    int actual_n_batch = llama_n_batch(context.get());
    batch = llama_batch_init(actual_n_batch, 0, 1);

    initialized = true;
    std::cout << "Embedder inicializado com sucesso. Dimensão: " << n_embd 
              << ", Batch size: " << actual_n_batch << std::endl;
    
    return true;
}

// Geração de embeddings
std::vector<float> Embedder::generate_embedding(const std::string& text) {
    if (!initialized) {
        throw std::runtime_error("Embedder não inicializado. Chame initialize() primeiro.");
    }
    std::vector<std::vector<float>> result = generate_embeddings({text});
    return !result.empty() ? result[0] : std::vector<float>();
}

std::vector<std::vector<float>> Embedder::generate_embeddings(const std::vector<std::string>& texts) {
    if (!initialized) {
        throw std::runtime_error("Embedder não inicializado. Chame initialize() primeiro.");
    }

    const int n_prompts = texts.size();
    if (n_prompts == 0) {
        return {};
    }

    std::vector<std::vector<float>> result;
    result.reserve(n_prompts);

    // Get maximum batch size allowed from config
    const int max_batch = config.n_batch;

    // Processar cada texto individualmente
    for (const auto& text : texts) {
        // Clear previous KV cache before each embedding
        if (context) {
            llama_memory_seq_rm(llama_get_memory(context.get()), 0, -1, -1);
        }
        
        // Tokenizar
        auto tokens = tokenize(text, true);
        if (tokens.empty()) {
            std::cerr << __func__ << ": failed to tokenize text" << std::endl;
            result.push_back(std::vector<float>(n_embd, 0.0f));
            continue;
        }

        // --- TRUNCATION ---
        if (tokens.size() > (size_t)max_batch) {
            tokens.resize(max_batch);
        }
        
        // Reset batch
        batch.n_tokens = static_cast<int32_t>(tokens.size());

        for (size_t i = 0; i < tokens.size(); i++) {
            batch.token[i] = tokens[i];
            batch.pos[i] = static_cast<llama_pos>(i);
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = (i == tokens.size() - 1); 
        }

        // Processar
        if (llama_decode(context.get(), batch) < 0) {
            std::cerr << __func__ << ": llama_decode failed" << std::endl;
            result.push_back(std::vector<float>(n_embd, 0.0f));
            continue;
        }

        const float* embd = llama_get_embeddings_seq(context.get(), 0);
        if (embd == nullptr) {
            embd = llama_get_embeddings_ith(context.get(), 0);
            if (embd == nullptr) {
                std::cerr << __func__ << ": failed to get embeddings" << std::endl;
                result.push_back(std::vector<float>(n_embd, 0.0f));
                continue;
            }
        }

        std::vector<float> embedding(embd, embd + n_embd);
        
        // Normalize logic
        if (config.embd_normalize > 0) {
            double sum = 0.0;
            for (float val : embedding) sum += val * val;
            const double norm = std::sqrt(sum);
            if (norm > 1e-6) {
                const double scale = (config.embd_normalize == 2) ? std::sqrt(n_embd) / norm : 1.0 / norm;
                for (float& val : embedding) val *= static_cast<float>(scale);
            }
        }
        result.push_back(embedding);
    }

    return result;
}

void Embedder::clear_embedding_cache() {
    if (context) {
        llama_memory_seq_rm(llama_get_memory(context.get()), 0, -1, -1);
    }
}

// Tokenização usando vocab_ptr
std::vector<llama_token> Embedder::tokenize(const std::string& text, bool add_special_tokens) const {
    if (!vocab_ptr) {
        std::cerr << __func__ << ": vocabulary not initialized" << std::endl;
        return {};
    }

    // Primeira passagem: obter número de tokens
    const int n_tokens = llama_tokenize(vocab_ptr, text.c_str(), static_cast<int32_t>(text.length()), nullptr, 0, add_special_tokens, true);
    
    if (n_tokens < 0) {
        // Buffer muito pequeno, alocar com tamanho necessário
        std::vector<llama_token> tokens(-n_tokens);
        int actual_tokens = llama_tokenize(vocab_ptr, text.c_str(), static_cast<int32_t>(text.length()), tokens.data(), static_cast<int32_t>(tokens.size()), add_special_tokens, true);
        if (actual_tokens < 0) {
            std::cerr << __func__ << ": tokenization failed" << std::endl;
            return {};
        }
        tokens.resize(actual_tokens);
        return tokens;
    } else {
        // Alocar com tamanho exato
        std::vector<llama_token> tokens(n_tokens);
        int actual_tokens = llama_tokenize(vocab_ptr, text.c_str(), static_cast<int32_t>(text.length()), tokens.data(), static_cast<int32_t>(tokens.size()), add_special_tokens, true);
        if (actual_tokens != n_tokens) {
            std::cerr << __func__ << ": token count mismatch: " << actual_tokens << " vs " << n_tokens << std::endl;
            tokens.resize(std::max(actual_tokens, 0));
        }
        return tokens;
    }
}


// Semantic search e funções de similaridade permanecem inalteradas
std::vector<std::tuple<double, int, std::string>> Embedder::semantic_search(
    const std::string& query, 
    const std::vector<std::string>& documents,
    int top_k) {
    
    if (!initialized) {
        throw std::runtime_error("Embedder não inicializado.");
    }
    
    auto query_embedding = generate_embedding(query);
    auto doc_embeddings = generate_embeddings(documents);
    
    std::vector<std::tuple<double, int, std::string>> similarities;
    for (size_t i = 0; i < documents.size(); ++i) {
        double sim = cosine_similarity(query_embedding, doc_embeddings[i]);
        similarities.emplace_back(sim, static_cast<int>(i), documents[i]);
    }
    
    std::sort(similarities.begin(), similarities.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a) > std::get<0>(b);
    });
    
    if (top_k > 0 && top_k < static_cast<int>(similarities.size())) {
        similarities.resize(top_k);
    }
    
    return similarities;
}

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

int Embedder::get_embedding_dimension() const {
    return initialized ? n_embd : 0;
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
bool Embedder::config_load(const std::string& config_path) {
    try {
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            return false;
        }
        
        json config_json;
        config_file >> config_json;
        
        if (config_json.contains("model_path")) config.model_path = config_json["model_path"];
        if (config_json.contains("embedding_dimension")) config.embedding_dimension = config_json["embedding_dimension"];
        if (config_json.contains("seed")) config.seed = config_json["seed"];
        if (config_json.contains("n_ctx")) config.n_ctx = config_json["n_ctx"];
        if (config_json.contains("n_batch")) config.n_batch = config_json["n_batch"];
        if (config_json.contains("n_threads")) config.n_threads = config_json["n_threads"];
        if (config_json.contains("n_gpu_layers")) config.n_gpu_layers = config_json["n_gpu_layers"];
        if (config_json.contains("use_mlock")) config.use_mlock = config_json["use_mlock"];
        if (config_json.contains("use_mmap")) config.use_mmap = config_json["use_mmap"];
        if (config_json.contains("f16_kv")) config.f16_kv = config_json["f16_kv"];
        if (config_json.contains("embd_normalize")) config.embd_normalize = config_json["embd_normalize"];
        
        return true;
    } catch (...) {
        return false;
    }
}

bool Embedder::save_config(const std::string& config_path) {
    try {
        json config_json;
        config_json["model_path"] = config.model_path;
        config_json["embedding_dimension"] = config.embedding_dimension;
        config_json["seed"] = config.seed;
        config_json["n_ctx"] = config.n_ctx;
        config_json["n_batch"] = config.n_batch;
        config_json["n_threads"] = config.n_threads;
        config_json["n_gpu_layers"] = config.n_gpu_layers;
        config_json["use_mlock"] = config.use_mlock;
        config_json["use_mmap"] = config.use_mmap;
        config_json["f16_kv"] = config.f16_kv;
        config_json["embd_normalize"] = config.embd_normalize;

        std::ofstream config_file(config_path);
        config_file << config_json.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

void Embedder::create_default_config() {
    config = Config(); 
    if (fs::exists("models/ggml-model-q4_0.gguf")) {
        config.model_path = "models/ggml-model-q4_0.gguf";
    }
    save_config(config_path);
}