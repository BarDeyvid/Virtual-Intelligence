#include "Embedding/Embedder.hpp"
#include <thread>
#include <cstring>
#include <cmath>

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
    this->config_path = config_path_;
    
    // Gerenciamento de Config
    fs::path config_dir = fs::path(config_path_).parent_path();
    if (!config_dir.empty() && !fs::exists(config_dir)) {
        fs::create_directories(config_dir);
    }
    
    if (!load_config(config_path_)) {
        std::cerr << "Falha ao carregar configuração. Criando configuração padrão..." << std::endl;
        create_default_config();
        if (!load_config(config_path_)) {
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
    if (vocab_ptr == nullptr) {
        std::cerr << __func__ << ": failed to get model vocabulary" << std::endl;
        return false;
    }

    // 4. Configurar parâmetros do CONTEXTO
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx       = config.n_ctx;
    cparams.n_batch     = config.n_batch;
    cparams.n_threads   = (config.n_threads <= 0) ? std::thread::hardware_concurrency() : config.n_threads;
    cparams.embeddings  = true;

    // 5. Criar o CONTEXTO
    llama_context * context_ptr = llama_init_from_model(model.get(), cparams);
    if (context_ptr == nullptr) {
        std::cerr << __func__ << ": failed to create context from model" << std::endl;
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

    // Processar cada texto individualmente
    for (const auto& text : texts) {
        // Tokenizar
        auto tokens = tokenize(text, true);
        if (tokens.empty()) {
            std::cerr << __func__ << ": failed to tokenize text" << std::endl;
            result.push_back(std::vector<float>(n_embd, 0.0f));
            continue;
        }

        // Preparar batch para este texto individual
        llama_batch_free(batch);
        batch = llama_batch_init(static_cast<int32_t>(tokens.size()), 0, 1);

        // Adicionar tokens ao batch - FIX: set logits correctly
        for (size_t i = 0; i < tokens.size(); i++) {
            batch.token[i] = tokens[i];
            batch.pos[i] = static_cast<llama_pos>(i);
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            
            // CRITICAL FIX: Set logits = true for at least one token to get embeddings
            // For sequence embeddings, we typically set the last token to have logits = true
            batch.logits[i] = (i == tokens.size() - 1); // Only last token gets logits
        }
        batch.n_tokens = static_cast<int32_t>(tokens.size());

        // Processar
        if (llama_decode(context.get(), batch) < 0) {
            std::cerr << __func__ << ": llama_decode failed" << std::endl;
            result.push_back(std::vector<float>(n_embd, 0.0f));
            continue;
        }

        // Obter embedding - FIX: Use correct function for embeddings
        // For encoder models with pooling, we get the sequence embedding directly
        const float* embd = llama_get_embeddings_seq(context.get(), 0);
        if (embd == nullptr) {
            // Fallback: try the alternative function
            embd = llama_get_embeddings_ith(context.get(), 0);
            if (embd == nullptr) {
                std::cerr << __func__ << ": failed to get embeddings" << std::endl;
                result.push_back(std::vector<float>(n_embd, 0.0f));
                continue;
            }
        }

        // Aplicar normalização se necessário
        std::vector<float> embedding(embd, embd + n_embd);
        if (config.embd_normalize > 0) {
            double sum = 0.0;
            for (float val : embedding) {
                sum += val * val;
            }
            const double norm = std::sqrt(sum);
            if (norm > 1e-6) {
                const double scale = (config.embd_normalize == 2) ? std::sqrt(n_embd) / norm : 1.0 / norm;
                for (float& val : embedding) {
                    val *= static_cast<float>(scale);
                }
            }
        }

        result.push_back(embedding);
    }

    return result;
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

bool Embedder::load_config(const std::string& config_path) {
    try {
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) return false;
        
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