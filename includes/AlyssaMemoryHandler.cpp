// AlyssaMemoryHandler.cpp 
#include <AlyssaMemoryHandler.hpp> 
#include "Embedding/Embedder.hpp"

using namespace alyssa_memory;

// ============================================================================
// Implementações das Estruturas
// ============================================================================

EmotionalState::EmotionalState(const std::string& n, double i) 
    : name(n), intensity(i) {
    timestamp = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
}

Intention::Intention(const std::string& desc, const std::string& t, const std::string& trig)
    : description(desc), type(t), trigger(trig), active(true), motivation(0.5) {
    created_at = Utils::getCurrentISOTime();
}

MemoryLink::MemoryLink(MemoryId src, MemoryId tgt, double w, const std::string& t)
    : source_id(src), target_id(tgt), weight(w), type(t) {}

Reflection::Reflection(MemoryId mem_id, const std::string& t, const std::string& c)
    : memory_id(mem_id), type(t), content(c) {
    created_at = Utils::getCurrentISOTime();
}
// ============================================================================
// Implementação de EmotionalAnalyzer
// ============================================================================

EmotionalAnalyzer::EmotionalAnalyzer(const EmotionalAnalyzerConfig& config) : config_(config) {
    // Inicializar categorias de emoções
    emotion_categories = config_.emotion_categories;
    
    // Inicializar pesos emocionais
    for (const auto& weight_pair : config_.default_weights) {
        emotion_weights[weight_pair.emotion] = weight_pair.weight;
    }
    
    initializeEmotionLexicons();
}

// TODO: Again, Make an Neural Network for this part too
void EmotionalAnalyzer::initializeEmotionLexicons() {
    emotion_lexicons = EmotionLexiconLoader::loadDefaultPortugueseLexicon();
}

EmotionalAnalysis EmotionalAnalyzer::analyzeText(const std::string& text) {
    EmotionalAnalysis result;
    std::unordered_map<std::string, double> scores;

    std::string normalized_text = TextNormalizer::toLowerCase(text);
    // normalized_text = TextNormalizer::normalizePortugueseAccents(normalized_text); TODO: create this
    
    // Inicializar scores
    for (const auto& emotion : emotion_categories) {
        scores[emotion] = 0.0;
    }
    
    // Análise baseada em lexicon
    analyzeWithLexicon(normalized_text, scores);
    
    // Análise baseada em padrões e intensificadores
    analyzePatterns(normalized_text, scores);
    
    // Análise de pontuação e exclamações
    analyzePunctuation(normalized_text, scores);
    
    // Normalizar scores e criar vector emocional
    result.emotion_scores = scores;
    result.emotional_vector = normalizeScores(scores);
    result.dominant_emotion = findDominantEmotion(scores);
    result.confidence = calculateConfidence(scores);
    
    return result;
}

EmotionalAnalysis EmotionalAnalyzer::analyzeConversation(const std::string& user_input, const std::string& ai_response) {
    // Combinar análise do input do usuário e resposta da IA
    auto user_analysis = analyzeText(user_input);
    auto ai_analysis = analyzeText(ai_response);
    
    // Combinar scores (dar mais peso ao input do usuário)
    EmotionalAnalysis combined;
    std::unordered_map<std::string, double> combined_scores;
    
    for (const auto& emotion : emotion_categories) {
        combined_scores[emotion] = (user_analysis.emotion_scores[emotion] * config_.user_input_weight) + 
                                  (ai_analysis.emotion_scores[emotion] * config_.ai_response_weight);
    }
    
    combined.emotion_scores = combined_scores;
    combined.emotional_vector = normalizeScores(combined_scores);
    combined.dominant_emotion = findDominantEmotion(combined_scores);
    combined.confidence = calculateConfidence(combined_scores);
    
    return combined;
}

void EmotionalAnalyzer::analyzeWithLexicon(const std::string& text, std::unordered_map<std::string, double>& scores) const {
    for (const auto& [emotion, words] : emotion_lexicons) {
        // Obter peso base da emocao
        double emotion_weight = emotion_weights.count(emotion) ? emotion_weights.at(emotion) : 1.0;
        for (const auto& word : words) {
            // Buscar palavra no texto (como palavra completa)
            std::regex word_regex("\\b" + word + "\\b", std::regex::icase);
            auto words_begin = std::sregex_iterator(text.begin(), text.end(), word_regex);
            auto words_end = std::sregex_iterator();
            
            int count = std::distance(words_begin, words_end);
            if (count > 0) {
                scores[emotion] += count * config_.lexicon_word_weight * emotion_weight;
            }
        }
    }
}

// TODO: Create Categorize Neural Network model for this one
void EmotionalAnalyzer::analyzePatterns(const std::string& text, std::unordered_map<std::string, double>& scores) const{
    // Detectar intensificadores
    std::vector<std::string> intensifiers = {"muito", "extremamente", "totalmente", "completamente", 
                                            "realmente", "verdadeiramente", "absolutamente"};
    
    for (const auto& intensifier : intensifiers) {
        std::regex intensifier_regex("\\b" + intensifier + "\\s+(\\w+)");
        auto matches_begin = std::sregex_iterator(text.begin(), text.end(), intensifier_regex);
        auto matches_end = std::sregex_iterator();
        
        for (auto it = matches_begin; it != matches_end; ++it) {
            std::string following_word = (*it)[1];
            // Verificar se a palavra seguinte está em algum lexicon
            for (const auto& [emotion, words] : emotion_lexicons) {
                if (std::find(words.begin(), words.end(), following_word) != words.end()) {
                    scores[emotion] += config_.intesifier_boost; 
                }
            }
        }
    }
    
    // Detectar negações
    std::vector<std::string> negations = {"não", "nem", "nunca", "jamais"};
    for (const auto& negation : negations) {
        std::regex negation_regex("\\b" + negation + "\\s+(\\w+)");
        auto matches_begin = std::sregex_iterator(text.begin(), text.end(), negation_regex);
        auto matches_end = std::sregex_iterator();
        
        for (auto it = matches_begin; it != matches_end; ++it) {
            std::string following_word = (*it)[1];
            for (const auto& [emotion, words] : emotion_lexicons) {
                if (std::find(words.begin(), words.end(), following_word) != words.end()) {
                    scores[emotion] -= config_.negation_penalty; 
                }
            }
        }
    }
}

// TODO: Create Categorize Neural Network model for this one too
void EmotionalAnalyzer::analyzePunctuation(const std::string& text, std::unordered_map<std::string, double>& scores) const {
    // Contar exclamações (aumenta intensidade emocional)
    int exclamation_count = std::count(text.begin(), text.end(), '!');
    if (exclamation_count > 0) {
        // Aumentar todas as emoções proporcionalmente
        for (auto& [emotion, score] : scores) {
            score += exclamation_count * config_.exclamation_boost;
        }
    }
    
    // Contar pontos de interrogação (aumenta surpresa/confusão)
    int question_count = std::count(text.begin(), text.end(), '?');
    if (question_count > 0) {
        scores["surpresa"] += question_count * config_.question_boost;
        scores["antecipacao"] += question_count * (config_.question_boost / 2.0); 
    }
    
    int period_count = std::count(text.begin(), text.end(), '.');
    if (period_count >= 3 && text.size() < 40) { // Ex: curtas frases com muitos pontos finais "Odeio!!!!"
        scores["raiva"] += config_.period_frustation_boost;
        scores["desgosto"] += config_.period_frustation_boost;
    }
}

std::vector<float> EmotionalAnalyzer::normalizeScores(const std::unordered_map<std::string, double>& scores) const {
    std::vector<float> vector;
    double max_score = 0.0;
    
    // Encontrar score máximo para normalização
    for (const auto& emotion : emotion_categories) {
        max_score = std::max(max_score, scores.at(emotion));
    }
    
    // Normalizar para [0, 1]
    for (const auto& emotion : emotion_categories) {
        float normalized_score = max_score > 0 ? scores.at(emotion) / max_score : 0.0f;
        vector.push_back(normalized_score);
    }
    
    return vector;
}

std::string EmotionalAnalyzer::findDominantEmotion(const std::unordered_map<std::string, double>& scores) const {
    std::string dominant = "neutral";
    double max_score = 0.0;
    
    for (const auto& [emotion, score] : scores) {
        if (score > max_score) {
            max_score = score;
            dominant = emotion;
        }
    }
    
    return max_score > config_.min_confidence_threshold ? dominant : "neutral";
}

double EmotionalAnalyzer::calculateConfidence(const std::unordered_map<std::string, double>& scores) const {
    double sum = 0.0;
    for (const auto& [emotion, score] : scores) {
        sum += score;
    }
    
    // Confiança baseada na intensidade geral das emoções detectadas
    return std::min(1.0, sum / 2.0);
}

// ============================================================================
// Implementação de Utils
// ============================================================================

std::vector<float> Utils::getTimeStamp() {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    double hours = std::fmod(ts / 3600.0, 24.0);
    double radians = hours * (M_PI / 12.0);
    
    return { 
        static_cast<float>(std::sin(radians)), 
        static_cast<float>(std::cos(radians)) 
    };
}

std::string Utils::getCurrentISOTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);
    
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buffer);
}

double Utils::sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

double Utils::normalizeImportance(double raw_importance) {
    return std::max(0.0, std::min(1.0, raw_importance));
}

// ============================================================================
// Implementação de LexiconScoreCalculator
// ============================================================================

void LexiconScoreCalculator::calculate(const std::string& text,
                                       std::unordered_map<std::string, double>& scores) {
    std::vector<std::string> words = TextNormalizer::extractWords(text);
    
    for (const auto& word : words) {
        for (const auto& [emotion, lexicon_words] : lexicons_) {
            // Check if the word exists in this emotion's lexicon
            if (std::find(lexicon_words.begin(), lexicon_words.end(), word) != lexicon_words.end()) {
                double weight = weights_.count(emotion) ? weights_.at(emotion) : 1.0;
                scores[emotion] += weight;
            }
        }
    }
}

// ============================================================================
// Implementação de TextNormalizer
// ============================================================================

// Initialize the static map (Best effort for single-byte, but we will use a string map for UTF-8 in the function)
const std::unordered_map<char, char> TextNormalizer::accent_map = {}; 

std::string TextNormalizer::toLowerCase(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) {return std::tolower(c);});
    return result;
}


std::string TextNormalizer::removeExtraSpaces(const std::string& text) {
    std::string result;
    bool space_found = false;
    
    for (char c : text) {
        if (std::isspace(c)) {
            if (!space_found) {
                result += ' ';
                space_found = true;
            }
        } else {
            result += c;
            space_found = false;
        }
    }
    
    // Trim leading/trailing spaces
    size_t first = result.find_first_not_of(' ');
    if (std::string::npos == first) return "";
    size_t last = result.find_last_not_of(' ');
    return result.substr(first, (last - first + 1));
}

std::string TextNormalizer::normalizePunctuation(const std::string& text) {
    std::string result = text;
    // Add spaces around punctuation to assist tokenization, except for common abbreviations if needed
    // Simple regex approach: replace punctuation with " <punct> "
    std::regex punct_re("([.,!?;:])");
    result = std::regex_replace(result, punct_re, " $1 ");
    return removeExtraSpaces(result);
}

std::string TextNormalizer::normalizePortugueseAccents(const std::string& text) {
    std::string result = text;
    
    // Map of UTF-8 sequences to ASCII equivalents
    // Note: C++ std::string handles these as multi-byte sequences
    static const std::vector<std::pair<std::string, std::string>> replacements = {
        {"á", "a"}, {"à", "a"}, {"ã", "a"}, {"â", "a"}, {"ä", "a"},
        {"Á", "a"}, {"À", "a"}, {"Ã", "a"}, {"Â", "a"}, {"Ä", "a"},
        {"é", "e"}, {"è", "e"}, {"ê", "e"}, {"ë", "e"},
        {"É", "e"}, {"È", "e"}, {"Ê", "e"}, {"Ë", "e"},
        {"í", "i"}, {"ì", "i"}, {"î", "i"}, {"ï", "i"},
        {"Í", "i"}, {"Ì", "i"}, {"Î", "i"}, {"Ï", "i"},
        {"ó", "o"}, {"ò", "o"}, {"õ", "o"}, {"ô", "o"}, {"ö", "o"},
        {"Ó", "o"}, {"Ò", "o"}, {"Õ", "o"}, {"Ô", "o"}, {"Ö", "o"},
        {"ú", "u"}, {"ù", "u"}, {"û", "u"}, {"ü", "u"},
        {"Ú", "u"}, {"Ù", "u"}, {"Û", "u"}, {"Ü", "u"},
        {"ç", "c"}, {"Ç", "c"}, {"ñ", "n"}, {"Ñ", "n"}
    };

    for (const auto& [accented, normalized] : replacements) {
        size_t pos = 0;
        while ((pos = result.find(accented, pos)) != std::string::npos) {
            result.replace(pos, accented.length(), normalized);
            pos += normalized.length();
        }
    }
    
    return result;
}

std::vector<std::string> TextNormalizer::extractWords(const std::string& text) {
    std::vector<std::string> words;
    std::string normalized = normalizePunctuation(text);
    normalized = toLowerCase(normalized);
    // Note: We might want to keep accents for semantic meaning, but remove them for lexicon matching
    // depending on how the lexicon is stored. Assuming lexicon is normalized:
    normalized = normalizePortugueseAccents(normalized); 
    
    std::stringstream ss(normalized);
    std::string word;
    while (ss >> word) {
        // Remove residual punctuation if any remains attached
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
        if (!word.empty()) {
            words.push_back(word);
        }
    }
    return words;
}

// ============================================================================
// Implementação de SQLStatementBuilder
// ============================================================================

std::string SQLStatementBuilder::escapeSQLString(const std::string& input) {
    std::string output = input;
    size_t pos = 0;
    while ((pos = output.find("'", pos)) != std::string::npos) {
        output.replace(pos, 1, "''");
        pos += 2;
    }
    return output;
}

std::string SQLStatementBuilder::createMemoryInsert(const std::string& content, 
                                     const std::string& context,
                                     const std::string& emotion,
                                     double importance,
                                     uint64_t timestamp,
                                     const std::string& vtime) {
    // Returns a parameterized query string. Values are bound later.
    return R"(
        INSERT INTO memories (conteudo, contexto, emocao, importancia, timestamp, vtime)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
}

std::string SQLStatementBuilder::createEmbeddingInsert(MemoryId memory_id,
                                        const std::vector<float>& embedding,
                                        const std::string& created_at) {
    // This matches the binding order in AdvancedMemorySystem::storeEmbedding
    // 1: memory_id, 2: embedding (blob), 3: size (int), 4: created_at
    return R"(
        INSERT INTO memory_embeddings (memory_id, embedding, embedding_dimension, created_at)
        VALUES (?, ?, ?, ?)
    )";
}

std::string SQLStatementBuilder::createSemanticSearchQuery() {
    // Selects all embeddings to compute cosine similarity in C++
    return R"(
        SELECT m.id, m.conteudo, m.emocao, m.importancia, me.embedding
        FROM memories m
        JOIN memory_embeddings me ON m.id = me.memory_id
        WHERE me.embedding IS NOT NULL
    )";
}

std::string SQLStatementBuilder::createContextualSearchQuery(const std::string& pattern, int limit) {
    return R"(
        SELECT id, conteudo, emocao, importancia 
        FROM memories 
        WHERE conteudo LIKE ? OR contexto LIKE ?
        ORDER BY importancia DESC 
        LIMIT ?
    )";
}

// ============================================================================
// Implementação de AdvancedMemorySystem
// ============================================================================

AdvancedMemorySystem::AdvancedMemorySystem(const std::string& db_path, std::shared_ptr<Embedder> embedder_ref) 
    : db(nullptr), embedder(embedder_ref) // <-- Recebe e armazena o embedder na lista de inicialização
{
    int rc = sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    initializeDatabase();
    loadCurrentState();
    
    emotion_weights = {
        {"alegria", 0.8}, {"tristeza", 0.6}, {"raiva", 0.7}, 
        {"medo", 0.5}, {"surpresa", 0.4}, {"confiança", 0.9}
    };
    
    // NÃO crie um novo embedder. Apenas verifique o que recebemos.
    if (!embedder || !embedder->is_initialized()) {
        std::cerr << "AVISO: Embedder recebido pelo AdvancedMemorySystem não está inicializado." << std::endl;
    } else {
        std::cout << "Embedder integrado com sucesso no AdvancedMemorySystem" << std::endl;
    }

    emotional_analyzer = std::make_unique<EmotionalAnalyzer>();
    std::cout << " Emotional Analyzer inicializado\n"; 
    std::cout << "Sistema de Memória Avançado Inicializado\n";
}

AdvancedMemorySystem::~AdvancedMemorySystem() {
    if (db) {
        saveCurrentState();
        sqlite3_close(db);
    }
}

void AdvancedMemorySystem::initializeDatabase() {
    const char* tables[] = {
        // Tabela de estados emocionais
        R"(
        CREATE TABLE IF NOT EXISTS memories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            
            -- Colunas necessárias para a lógica do AdvancedMemorySystem
            conteudo TEXT NOT NULL,
            contexto TEXT,
            emocao TEXT DEFAULT 'neutral',
            importancia REAL DEFAULT 0.5,
            timestamp INTEGER,
            vtime TEXT,
            last_access INTEGER,
            access_count INTEGER DEFAULT 0,

            -- Novas colunas para tokens (intenção do usuário)
            model_hash INTEGER, 
            n_tokens INTEGER,
            emo_dim INTEGER,
            tokens BLOB
        );
        )",
        R"(
        CREATE TABLE IF NOT EXISTS emotional_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            intensity REAL DEFAULT 0.5,
            timestamp INTEGER
        );
        )",
        
        // Tabela de intenções
        R"(
        CREATE TABLE IF NOT EXISTS intentions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            description TEXT NOT NULL,
            type TEXT NOT NULL,
            trigger TEXT,
            active BOOLEAN DEFAULT 1,
            motivation REAL DEFAULT 0.5,
            created_at TEXT
        );
        )",
        
        // Tabela de decadência de memórias
        R"(
        CREATE TABLE IF NOT EXISTS memory_decay (
            memory_id INTEGER PRIMARY KEY,
            last_access INTEGER,
            current_value REAL,
            decaying_since INTEGER,
            FOREIGN KEY(memory_id) REFERENCES memories(id)
        );
        )",
        
        // Tabela de reflexões
        R"(
        CREATE TABLE IF NOT EXISTS reflections (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            memory_id INTEGER,
            type TEXT,
            content TEXT,
            created_at TEXT,
            FOREIGN KEY(memory_id) REFERENCES memories(id)
        );
        )",
        
        // Tabela de vínculos entre memórias
        R"(
        CREATE TABLE IF NOT EXISTS memory_links (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_id INTEGER,
            target_id INTEGER,
            weight REAL DEFAULT 1.0,
            type TEXT,
            FOREIGN KEY(source_id) REFERENCES memories(id),
            FOREIGN KEY(target_id) REFERENCES memories(id)
        );
        )",
        
        // NOVA: Tabela de embeddings vetoriais
        R"(
        CREATE TABLE IF NOT EXISTS memory_embeddings (
            memory_id INTEGER PRIMARY KEY,
            embedding BLOB,
            embedding_dimension INTEGER,
            created_at TEXT,
            FOREIGN KEY(memory_id) REFERENCES memories(id)
        );
        )"
    };
    
    char* errMsg = nullptr;
    for (const char* table_sql : tables) {
        if (sqlite3_exec(db, table_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "SQL error: ";
            error += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(error);
        }
    }
}

void AdvancedMemorySystem::loadCurrentState() {
    const char* sql = "SELECT name, intensity FROM emotional_states ORDER BY timestamp DESC LIMIT 1";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_emotional_state.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            current_emotional_state.intensity = sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    
    loadActiveIntentions();
}

void AdvancedMemorySystem::saveCurrentState() {
    const char* sql = "INSERT INTO emotional_states (name, intensity, timestamp) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, current_emotional_state.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, current_emotional_state.intensity);
        sqlite3_bind_int64(stmt, 3, current_emotional_state.timestamp);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void AdvancedMemorySystem::loadActiveIntentions() {
    active_intentions.clear();
    const char* sql = "SELECT id, description, type, trigger, motivation, created_at FROM intentions WHERE active = 1";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Intention intention(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
            );
            intention.id = sqlite3_column_int(stmt, 0);
            intention.motivation = sqlite3_column_double(stmt, 4);
            intention.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            active_intentions.push_back(intention);
        }
        sqlite3_finalize(stmt);
    }
}

EmotionalState AlyssaMemoryManager::getCurrentEmotionalState() const {
    return memory_system->getCurrentEmotionalState();
}

std::vector<Intention> AlyssaMemoryManager::getActiveIntentions() const {
    return memory_system->getActiveIntentions();
}

// === MÉTODOS DE EMBEDDING E BUSCA SEMÂNTICA ===

bool AdvancedMemorySystem::hasEmbedder() const {
    return embedder != nullptr && embedder->is_initialized();
}

// Gerar embedding para texto e salvar no banco
bool AdvancedMemorySystem::generateAndStoreEmbedding(int memory_id, const std::string& content) {
    if (!hasEmbedder()) {
        return false;
    }
    if (!embedder->is_initialized()){
        embedder->initialize("config\\embedder_config.json");
    }
    try {
        auto embedding = embedder->generate_embedding(content);
        storeEmbedding(memory_id, embedding);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error generating embedding: " << e.what() << std::endl;
        return false;
    }
}


std::vector<AdvancedMemorySystem::SemanticMemory> AdvancedMemorySystem::semanticSearch(const std::string& query, int top_k) {
    std::vector<SemanticMemory> results;
    
    if (!hasEmbedder()) {
        std::cerr << "Embedder not available for semantic search" << std::endl;
        return results;
    }
    
    try {
        auto query_embedding = embedder->generate_embedding(query);
        return semanticSearchWithEmbedding(query_embedding, top_k);
    } catch (const std::exception& e) {
        std::cerr << "Error in semantic search: " << e.what() << std::endl;
        return results;
    }
}

std::vector<AdvancedMemorySystem::SemanticMemory> AdvancedMemorySystem::semanticSearchWithEmbedding(const std::vector<float>& query_embedding, int top_k) {
    std::vector<SemanticMemory> results;
    
    const char* sql = R"(
        SELECT m.id, m.conteudo, m.emocao, m.importancia, me.embedding
        FROM memories m
        JOIN memory_embeddings me ON m.id = me.memory_id
        WHERE me.embedding IS NOT NULL
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SemanticMemory mem;
            mem.memory_id = sqlite3_column_int(stmt, 0);
            mem.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            mem.emotion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            mem.importance = sqlite3_column_double(stmt, 3);
            
            // Extrair embedding do BLOB
            const void* blob_data = sqlite3_column_blob(stmt, 4);
            int blob_size = sqlite3_column_bytes(stmt, 4);
            std::vector<float> memory_embedding(blob_size / sizeof(float));
            memcpy(memory_embedding.data(), blob_data, blob_size);
            
            // Calcular similaridade
            mem.similarity_score = Embedder::cosine_similarity(query_embedding, memory_embedding);
            
            // Aplicar boosting contextual
            mem.similarity_score += calculateIntentionBoost(mem.content);
            mem.similarity_score += calculateEmotionalBoost(mem.emotion);
            
            results.push_back(mem);
        }
        sqlite3_finalize(stmt);
    }
    
    // Ordenar por similaridade
    std::sort(results.begin(), results.end(), 
        [](const SemanticMemory& a, const SemanticMemory& b) {
            return a.similarity_score > b.similarity_score;
        });
    
    if (results.size() > (size_t)top_k) {
        results.resize(top_k);
    }
    
    return results;
}

std::vector<AdvancedMemorySystem::HybridMemoryResult> AdvancedMemorySystem::hybridSearch(const std::string& query, int top_k) {
    std::vector<HybridMemoryResult> results;
    
    // Busca textual tradicional
    auto text_results = searchContextualMemories(query, top_k * 2);
    
    // Busca semântica
    auto semantic_results = semanticSearch(query, top_k * 2);
    
    // Combinar resultados
    std::unordered_map<int, HybridMemoryResult> combined;
    
    // Adicionar resultados textuais
    for (const auto& text_mem : text_results) {
        HybridMemoryResult result;
        result.memory_id = text_mem.id;
        result.content = text_mem.content;
        result.text_score = text_mem.relevance_score;
        result.semantic_score = 0.0;
        result.emotion = text_mem.emotion;
        combined[text_mem.id] = result;
    }
    
    // Adicionar/atualizar com resultados semânticos
    for (const auto& semantic_mem : semantic_results) {
        if (combined.find(semantic_mem.memory_id) != combined.end()) {
            combined[semantic_mem.memory_id].semantic_score = semantic_mem.similarity_score;
        } else {
            HybridMemoryResult result;
            result.memory_id = semantic_mem.memory_id;
            result.content = semantic_mem.content;
            result.text_score = 0.0;
            result.semantic_score = semantic_mem.similarity_score;
            result.emotion = semantic_mem.emotion;
            combined[semantic_mem.memory_id] = result;
        }
    }
    
    // Calcular scores combinados e coletar
    for (auto& [id, result] : combined) {
        result.combined_score = (result.text_score * 0.4) + (result.semantic_score * 0.6);
        results.push_back(result);
    }
    
    // Ordenar por score combinado
    std::sort(results.begin(), results.end(),
        [](const HybridMemoryResult& a, const HybridMemoryResult& b) {
            return a.combined_score > b.combined_score;
        });
    
    if (results.size() > (size_t)top_k) {
        results.resize(top_k);
    }
    
    return results;
}

void AdvancedMemorySystem::storeEmbedding(int memory_id, const std::vector<float>& embedding) {
    std::string sql_statment = SQLStatementBuilder::createEmbeddingInsert(static_cast<MemoryId>(memory_id), embedding, Utils::getCurrentISOTime());
    const char* sql = sql_statment.c_str();
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Converter vector<float> para BLOB
        sqlite3_bind_int(stmt, 1, memory_id);
        sqlite3_bind_blob(stmt, 2, embedding.data(), embedding.size() * sizeof(float), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, static_cast<int>(embedding.size()));
        sqlite3_bind_text(stmt, 4, Utils::getCurrentISOTime().c_str(), -1, SQLITE_STATIC);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// === INTERFACE HIGH-LEVEL PARA INTEGRAÇÃO ===

void AdvancedMemorySystem::setEmotionalState(const std::string& emotion, double intensity) {
    current_emotional_state.name = emotion;
    current_emotional_state.intensity = Utils::normalizeImportance(intensity);
    current_emotional_state.timestamp = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    
    std::cout << "Estado Emocional Atualizado: " << emotion 
              << " (intensidade: " << intensity << ")\n";
    
    checkEmotionalAutoActivation();
}

EmotionalState AdvancedMemorySystem::getCurrentEmotionalState() const {
    return current_emotional_state;
}

void AdvancedMemorySystem::applyMemoryDecay() {
    const char* sql = R"(
        UPDATE memory_decay 
        SET current_value = MAX(?, current_value - ?),
            last_access = ?
        WHERE current_value > ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
        sqlite3_bind_double(stmt, 1, MIN_IMPORTANCE);
        sqlite3_bind_double(stmt, 2, DECAY_PER_HOUR);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_bind_double(stmt, 4, MIN_IMPORTANCE);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Erro ao aplicar decadência: " << sqlite3_errmsg(db) << "\n";
        } else {
            std::cout << "Decadência de memórias aplicada\n";
        }
        sqlite3_finalize(stmt);
    }
}

void AdvancedMemorySystem::activateIntention(const std::string& description, const std::string& type, 
                      const std::string& trigger, double motivation_boost) {
    for (const auto& intention : active_intentions) {
        if (intention.description == description && intention.active) {
            std::cout << "Intenção já está ativa: " << description << "\n";
            return;
        }
    }
    
    const char* sql = R"(
        INSERT INTO intentions (description, type, trigger, active, motivation, created_at)
        VALUES (?, ?, ?, 1, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, trigger.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, 0.5 + motivation_boost);
        sqlite3_bind_text(stmt, 5, Utils::getCurrentISOTime().c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            loadActiveIntentions();
            std::cout << "Nova Intenção Ativada: " << description << "\n";
        }
        sqlite3_finalize(stmt);
    }
}

void AdvancedMemorySystem::deactivateIntention(int intention_id) {
    const char* sql = "UPDATE intentions SET active = 0 WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, intention_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        active_intentions.erase(
            std::remove_if(active_intentions.begin(), active_intentions.end(),
                [intention_id](const Intention& i) { return i.id == intention_id; }),
            active_intentions.end()
        );
        
        std::cout << "Intenção Desativada: ID " << intention_id << "\n";
    }
}

std::vector<Intention> AdvancedMemorySystem::getActiveIntentions() const {
    return active_intentions;
}

void AdvancedMemorySystem::generateReflections() {
    const char* sql = R"(
        SELECT m.id, m.conteudo, m.emocao, m.importancia 
        FROM memories m
        WHERE m.timestamp >= ? AND m.importancia >= 0.5
        ORDER BY m.timestamp DESC
    )";
    
    sqlite3_stmt* stmt;
    auto six_hours_ago = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now() - std::chrono::hours(6));
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, six_hours_ago);
        
        std::unordered_map<std::string, int> emotion_counts;
        std::unordered_map<std::string, int> memory_ids;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string emotion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            int memory_id = sqlite3_column_int(stmt, 0);
            
            emotion_counts[emotion]++;
            if (memory_ids.find(emotion) == memory_ids.end()) {
                memory_ids[emotion] = memory_id;
            }
        }
        sqlite3_finalize(stmt);
        
        for (const auto& [emotion, count] : emotion_counts) {
            if (count >= 3) {
                std::string reflection_content = 
                    "Notei que me senti muito " + emotion + 
                    " recentemente. Talvez isso signifique algo importante.";
                
                saveReflection(memory_ids[emotion], "emotional_pattern", reflection_content);
                std::cout << "Reflexão Gerada: " << reflection_content << "\n";
            }
        }
    }
}

void AdvancedMemorySystem::createMemoryLink(MemoryId source_id, MemoryId target_id, double weight, 
                     const std::string& link_type) {
    const char* sql = R"(
        INSERT INTO memory_links (source_id, target_id, weight, type)
        VALUES (?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, source_id);
        sqlite3_bind_int(stmt, 2, target_id);
        sqlite3_bind_double(stmt, 3, weight);
        sqlite3_bind_text(stmt, 4, link_type.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << "Vínculo criado: " << source_id << " → " << target_id 
                      << " (" << link_type << ")\n";
        }
        sqlite3_finalize(stmt);
    }
}

void AdvancedMemorySystem::linkMemoryToIntention(int memory_id, int intention_id) {
    std::string content = "Linked explicitly to intention ID: " + std::to_string(intention_id);
    saveReflection(memory_id, "intention_link", content);
    std::cout << "🔗 Memória " << memory_id << " vinculada à intenção " << intention_id << "\n";
}

std::vector<MemoryLink> AdvancedMemorySystem::getMemoryLinks(MemoryId memory_id) {
    std::vector<MemoryLink> links;
    const char* sql = R"(
        SELECT source_id, target_id, weight, type 
        FROM memory_links 
        WHERE source_id = ? OR target_id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, memory_id);
        sqlite3_bind_int(stmt, 2, memory_id);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MemoryLink link(
                static_cast<MemoryId>(sqlite3_column_int(stmt, 0)),
                static_cast<MemoryId>(sqlite3_column_int(stmt, 1)),
                sqlite3_column_double(stmt, 2),
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)))
            );
            links.push_back(link);
        }
        sqlite3_finalize(stmt);
    }
    return links;
}

std::vector<AdvancedMemorySystem::ContextualMemory> AdvancedMemorySystem::searchContextualMemories(const std::string& query, 
                                                      int top_k) {
    std::vector<ContextualMemory> results;
    
    const char* sql = R"(
        SELECT id, conteudo, emocao, importancia 
        FROM memories 
        WHERE conteudo LIKE ? OR contexto LIKE ?
        ORDER BY importancia DESC 
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string like_query = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, like_query.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, like_query.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, top_k * 2);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ContextualMemory mem;
            mem.id = sqlite3_column_int(stmt, 0);
            mem.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            mem.emotion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            mem.relevance_score = sqlite3_column_double(stmt, 3);
            
            mem.relevance_score += calculateIntentionBoost(mem.content);
            mem.relevance_score += calculateEmotionalBoost(mem.emotion);
            
            results.push_back(mem);
        }
        sqlite3_finalize(stmt);
    }
    
    std::sort(results.begin(), results.end(), 
        [](const ContextualMemory& a, const ContextualMemory& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    if (results.size() > (size_t)top_k) {
        results.resize(top_k);
    }
    
    return results;
}

// --- NOVOS MÉTODOS PARA ANÁLISE EMOCIONAL AUTOMÁTICA --- (ADICIONADOS)

EmotionalAnalysis AdvancedMemorySystem::analyzeEmotionalContent(const std::string& text) {
    return emotional_analyzer->analyzeText(text);
}

EmotionalAnalysis AdvancedMemorySystem::analyzeConversationEmotions(const std::string& user_input, const std::string& ai_response) {
    return emotional_analyzer->analyzeConversation(user_input, ai_response);
}

// Método para processar e armazenar memória com análise emocional automática
int AdvancedMemorySystem::storeMemoryWithEmotionalAnalysis(const std::string& content, const std::string& context) {
    // Analisar conteúdo emocionalmente
    auto emotional_analysis = analyzeEmotionalContent(content);
    
    const char* sql = R"(
        INSERT INTO memories (conteudo, contexto, emocao, importancia, timestamp, vtime)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int memory_id = -1;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        auto timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto vtime = Utils::getTimeStamp();
        std::string vtime_str;
        for (const auto& val : vtime) {
            vtime_str += std::to_string(val) + ",";
        }
        if (!vtime_str.empty()) vtime_str.pop_back();
        
        double importance = calculateAutomaticImportance(content, emotional_analysis);
        
        sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, context.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, emotional_analysis.dominant_emotion.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, importance);
        sqlite3_bind_int64(stmt, 5, timestamp);
        sqlite3_bind_text(stmt, 6, vtime_str.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            memory_id = sqlite3_last_insert_rowid(db);
            
            // Gerar embedding se disponível
            if (hasEmbedder()) {
                generateAndStoreEmbedding(memory_id, content);
            }
            
            std::cout << "💾 Memória armazenada (ID: " << memory_id 
                      << ") - Emoção: " << emotional_analysis.dominant_emotion 
                      << " (confiança: " << emotional_analysis.confidence << ")\n";
        } else {
             std::cerr << "Erro ao inserir memória: " << sqlite3_errmsg(db) << "\n";
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Erro ao preparar SQL para inserir memória: " << sqlite3_errmsg(db) << "\n";
    }
    
    return memory_id;
}

int AlyssaMemoryManager::storeMemoryWithEmotionalAnalysis(const std::string& content, const std::string& context) {
    if (!memory_system) return -1;
    return memory_system->storeMemoryWithEmotionalAnalysis(content, context);
}

void AlyssaMemoryManager::linkMemoryToIntention(int memory_id, int intention_id) {
    if (memory_system) {
        memory_system->linkMemoryToIntention(memory_id, intention_id);
    }
}

// Método para obter emotional_vector automaticamente para integração
std::vector<float> AdvancedMemorySystem::getAutoEmotionalVector(const std::string& text) {
    auto analysis = analyzeEmotionalContent(text);
    return analysis.emotional_vector;
}

double AdvancedMemorySystem::calculateAutomaticImportance(const std::string& content, const EmotionalAnalysis& analysis) {
    double importance = 0.5; // Importância base
    
    // Aumentar importância baseado na intensidade emocional
    double emotional_intensity = 0.0;
    for (const auto& score : analysis.emotional_vector) {
        emotional_intensity += score;
    }
    if (!analysis.emotional_vector.empty()) {
        emotional_intensity /= analysis.emotional_vector.size();
    }
    
    importance += emotional_intensity * 0.3;
    
    // Aumentar importância para conteúdo mais longo (potencialmente mais significativo)
    double length_factor = std::min(1.0, content.length() / 500.0);
    importance += length_factor * 0.2;
    
    // Aumentar importância para emoções fortes específicas
    if (analysis.dominant_emotion == "raiva" || analysis.dominant_emotion == "alegria") {
        importance += 0.1;
    }
    
    return std::min(1.0, importance);
}

void AdvancedMemorySystem::checkEmotionalAutoActivation() {
    auto it = emotion_to_intention.find(current_emotional_state.name);
    if (it != emotion_to_intention.end() && current_emotional_state.intensity > 0.7) {
        const auto& [description, type] = it->second;
        
        bool already_active = false;
        for (const auto& intention : active_intentions) {
            if (intention.description == description) {
                already_active = true;
                break;
            }
        }
        
        if (!already_active) {
            std::string trigger = "emoção: " + current_emotional_state.name;
            activateIntention(description, type, trigger, 0.3);
            std::cout << "Auto-ativação por emoção: " << description << "\n";
        }
    }
}

void AdvancedMemorySystem::saveReflection(int memory_id, const std::string& type, const std::string& content) {
    const char* sql = R"(
        INSERT INTO reflections (memory_id, type, content, created_at)
        VALUES (?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, memory_id);
        sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, Utils::getCurrentISOTime().c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

double AdvancedMemorySystem::calculateIntentionBoost(const std::string& content) {
    double boost = 0.0;
    for (const auto& intention : active_intentions) {
        if (content.find(intention.description) != std::string::npos) {
            boost += 0.3 * intention.motivation;
        }
    }
    return std::min(boost, 0.5);
}

double AdvancedMemorySystem::calculateEmotionalBoost(const std::string& memory_emotion) {
    if (memory_emotion == current_emotional_state.name) {
        return 0.2 * current_emotional_state.intensity;
    }
    return 0.0;
}

// === MÉTODOS DE MONITORAMENTO E DEBUG ===

void AdvancedMemorySystem::printSystemStatus() {
    std::cout << "\n=== STATUS DO SISTEMA DE MEMÓRIA ===\n";
    std::cout << "Estado Emocional: " << current_emotional_state.name 
              << " (intensidade: " << current_emotional_state.intensity << ")\n";
    std::cout << "Intenções Ativas: " << active_intentions.size() << "\n";
    
    for (const auto& intention : active_intentions) {
        std::cout << "   • " << intention.description 
                  << " [" << intention.type << "]"
                  << " (motivação: " << intention.motivation << ")\n";
    }
    
    // Contar memórias
    const char* sql = "SELECT COUNT(*) FROM memories";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            std::cout << "Total de Memórias: " << sqlite3_column_int(stmt, 0) << "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    // Status do embedder
    std::cout << "Embedder: " << (hasEmbedder() ? "Ativo" : "Inativo") << "\n";
    
    std::cout << "====================================\n\n";
}

// Novo método para demonstrar busca semântica
void AdvancedMemorySystem::demonstrateSemanticSearch(const std::string& query) {
    if (!hasEmbedder()) {
        std::cout << " Embedder não disponível para busca semântica\n";
        return;
    }
    
    std::cout << "\n🔍 DEMONSTRAÇÃO DE BUSCA SEMÂNTICA\n";
    std::cout << "Query: \"" << query << "\"\n";
    
    auto results = semanticSearch(query, 3);
    
    if (results.empty()) {
        std::cout << "Nenhum resultado encontrado.\n";
        return;
    }
    
    std::cout << "Top " << results.size() << " resultados:\n";
    for (const auto& mem : results) {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[" << mem.similarity_score << "] ";
        std::cout << "ID " << mem.memory_id << ": " << mem.content.substr(0, 80);
        if (mem.content.length() > 80) std::cout << "...";
        std::cout << " [" << mem.emotion << "]\n";
    }
    std::cout << "----------------------------------------\n";
}


// ============================================================================
// Implementação de AlyssaMemoryManager
// ============================================================================

AlyssaMemoryManager::AlyssaMemoryManager(const std::string& db_path, std::shared_ptr<Embedder> embedder_ref) {
    memory_system = std::make_unique<AdvancedMemorySystem>(db_path, embedder_ref);
}

// NOVA VERSÃO: gera emotional_vector automaticamente
void AlyssaMemoryManager::processInteraction(const std::string& user_input, 
                      const std::string& ai_response) { // REMOVER parâmetro emotional_vector
    
    // ANÁLISE EMOCIONAL AUTOMÁTICA (NOVO)
    auto emotional_analysis = memory_system->analyzeConversationEmotions(user_input, ai_response);
    
    // Usar análise automática para definir estado emocional
    memory_system->setEmotionalState(
        emotional_analysis.dominant_emotion, 
        emotional_analysis.confidence
    );
    
    // Armazenar memória com análise emocional automática
    int memory_id = memory_system->storeMemoryWithEmotionalAnalysis(
        user_input + " | " + ai_response, 
        "conversation"
    );
    if (memory_id != -1) {
        std::cout << "Memória da interação armazenada com ID: " << memory_id << "\n";
    }
    analyzeInputForIntentions(user_input);
    
    static int interaction_count = 0;
    if (++interaction_count % 10 == 0) {
        memory_system->applyMemoryDecay();
        memory_system->generateReflections();
    }
    
    if (interaction_count % 5 == 0) {
        memory_system->printSystemStatus();
    }
    
    // Demonstração de busca semântica
    if (interaction_count % 7 == 0) {
        memory_system->demonstrateSemanticSearch(user_input);
    }
    
    // Mostrar análise emocional (para debug)
    if (interaction_count % 3 == 0) {
        printEmotionalAnalysis(emotional_analysis, user_input);
    }
}

void AlyssaMemoryManager::processIdentityFact(const std::string& fact_value, const std::string& fact_type) {
    if (memory_system) {
        memory_system->activateIntention(
            "Identificar " + fact_type + " como " + fact_value,
            "identity",
            "fact: " + fact_type,
            0.5
        );
        std::cout << "[IDENTITY] Processado: " << fact_type << " = " << fact_value << std::endl;
    } else {
        std::cerr << "[ERROR] Memory system not initialized. Cannot process identity fact." << std::endl;
    }
}

// Método para compatibilidade com código legado
void AlyssaMemoryManager::processInteraction(const std::string& user_input, 
                      const std::string& ai_response,
                      const std::vector<float>& emotional_vector) {
    // Se emotional_vector for fornecido, usar ele, senão usar análise automática
    if (!emotional_vector.empty() && 
        std::accumulate(emotional_vector.begin(), emotional_vector.end(), 0.0) > 0.1) {
        // Usar vector fornecido (código legado)
        std::vector<std::string> emotions = {"alegria", "tristeza", "raiva", "medo", "surpresa"};
        std::string dominant_emotion = "neutral";
        double max_intensity = 0.0;
        
        for (size_t i = 0; i < emotional_vector.size() && i < emotions.size(); ++i) {
            if (emotional_vector[i] > max_intensity) {
                max_intensity = emotional_vector[i];
                dominant_emotion = emotions[i];
            }
        }
        
        memory_system->setEmotionalState(dominant_emotion, max_intensity);
    } else {
        // Usar análise automática
        processInteraction(user_input, ai_response);
        return;
    }
    
    // Resto do processamento...
    analyzeInputForIntentions(user_input);
    
    static int interaction_count = 0;
    if (++interaction_count % 10 == 0) {
        memory_system->applyMemoryDecay();
        memory_system->generateReflections();
    }
}

std::vector<AdvancedMemorySystem::ContextualMemory> AlyssaMemoryManager::getRelevantMemories(const std::string& context) {
    return memory_system->searchContextualMemories(context, 3);
}

std::vector<AdvancedMemorySystem::SemanticMemory> AlyssaMemoryManager::getSemanticMemories(const std::string& context) {
    return memory_system->semanticSearch(context, 3);
}

std::vector<AdvancedMemorySystem::HybridMemoryResult> AlyssaMemoryManager::getHybridMemories(const std::string& context) {
    return memory_system->hybridSearch(context, 3);
}

void AlyssaMemoryManager::setCurrentGoal(const std::string& goal, const std::string& type) {
    memory_system->activateIntention(goal, type, "user_defined", 0.5);
}

void AlyssaMemoryManager::printEmotionalAnalysis(const EmotionalAnalysis& analysis, const std::string& text) {
    std::cout << "\n🎭 ANÁLISE EMOCIONAL AUTOMÁTICA\n";
    std::cout << "Texto: \"" << text.substr(0, 50) << "...\"\n";
    std::cout << "Emoção dominante: " << analysis.dominant_emotion 
              << " (confiança: " << std::fixed << std::setprecision(2) << analysis.confidence << ")\n";
    
    std::cout << "Scores: ";
    // Precisa acessar as categorias de emoção. Vamos adicionar isso à struct ou pegar do analyzer.
    // Solução rápida: usar o map de scores.
    for (const auto& [emotion, score] : analysis.emotion_scores) {
         if (score > 0.05) { // Mostrar scores acima de um limiar
             std::cout << emotion << ": " 
                       << std::setprecision(2) << score << "  ";
         }
    }
    std::cout << "\n----------------------------------------\n";
}

void AlyssaMemoryManager::analyzeInputForIntentions(const std::string& input) {
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), 
        [](unsigned char c){ return std::tolower(c); }
    );
    
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>> triggers = {
        {"aprender", {"Aprender novo tópico", "aprendizado"}},
        {"como funciona", {"Entender mecanismos", "curiosidade"}},
        {"problema", {"Resolver desafio", "resolução"}},
        {"lembrar", {"Recuperar informação", "memória"}}
    };
    
    for (const auto& [keyword, intention] : triggers) {
        if (lower_input.find(keyword) != std::string::npos) {
            memory_system->activateIntention(intention.first, intention.second, keyword, 0.2);
            break;
        }
    }
}

// ============================================================================
// Main
// ============================================================================

// int main() {
// try {
//         AlyssaMemoryManager memory_manager("alyssa_advanced_memory.db", true);
//         
//         std::vector<std::pair<std::string, std::string>> interactions = {
//             {"Olá Alyssa, estou muito feliz hoje!", "Que ótimo! Fico contente em ouvir isso!"},
//             {"Estou extremamente frustrado com este código...", "Entendo sua frustração. Posso ajudar a resolver o problema?"},
//             {"Tenho medo de não conseguir terminar este projeto a tempo", "Não se preocupe, vamos planejar juntos!"},
//             {"Que surpresa agradável ver você funcionando tão bem!", "Obrigada! Fico feliz em surpreendê-la positivamente!"},
//             {"Estou com nojo dessa situação toda", "Sinto muito que esteja passando por isso. Quer conversar sobre?"}
//         };
//         
//         for (size_t i = 0; i < interactions.size(); ++i) {
//             std::cout << "\n=== Interação " << (i + 1) << " ===\n";
//             std::cout << "Usuário: " << interactions[i].first << "\n";
//             std::cout << "Alyssa: " << interactions[i].second << "\n";
//             
//             // AGORA SEM PRECISAR PASSAR emotional_vector MANUALMENTE!
//             memory_manager.processInteraction(
//                 interactions[i].first, 
//                 interactions[i].second
//             );
//         }
//         
//     } catch (const std::exception& e) {
//         std::cerr << "Erro: " << e.what() << std::endl;
//         return 1;
//     }
//     
//     return 0;
// }