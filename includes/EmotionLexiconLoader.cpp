#include "EmotionLexiconLoader.hpp"

namespace alyssa_memory {
    const std::string EmotionLexiconLoader::DEFAULT_LEXICON_PATH = "config/LexiconPortuguese.json";

    std::unordered_map<std::string, std::vector<std::string>> EmotionLexiconLoader::loadDefaultPortugueseLexicon() {        
        std::cout << "Tentando carregar o lexicon padrao do caminho: " << DEFAULT_LEXICON_PATH << std::endl;
        
        std::unordered_map<std::string, std::vector<std::string>> emotion_lexicons = loadLexiconFromFile(DEFAULT_LEXICON_PATH);

        if (emotion_lexicons.empty()) {
            std::cerr << "AVISO: Falha ao carregar o lexicon padrao do arquivo. O mapa esta vazio." << std::endl;
        }

        return emotion_lexicons;
    }

    std::unordered_map<std::string, std::vector<std::string>> EmotionLexiconLoader::loadLexiconFromFile(const std::string& filepath) {
        std::unordered_map<std::string, std::vector<std::string>> lexicon;
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "Erro: Nao foi possivel abrir o arquivo JSON em: " << filepath << std::endl;
            return lexicon;
        }

        try {
            json j;
            file >> j;

            for (auto it = j.begin(); it != j.end(); ++it) {
                std::string emotion = it.key();

                if (it->is_array()) {
                    lexicon[emotion] = it.value().get<std::vector<std::string>>();
                } else {
                    std::cerr << "Aviso: O valor para a emocao '" << emotion << "' nao e um array no JSON." << std::endl;
                }
            }

        } catch (json::parse_error& e) {
            std::cerr << "Erro de parsing JSON em " << filepath << ": " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Erro inesperado ao carregar o lexicon: " << e.what() << std::endl;
        }

        return lexicon;
    }

    void EmotionLexiconLoader::saveLexiconToFile(
        const std::unordered_map<std::string, std::vector<std::string>>& lexicon,
        const std::string& filepath) {
        
        json j(lexicon);
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Erro: Nao foi possivel criar/abrir o arquivo JSON para salvar em: " << filepath << std::endl;
            return;
        }

        try {
            file << j.dump(2);
        } catch (const std::exception& e) {
            std::cerr << "Erro ao salvar o lexicon para o arquivo: " << e.what() << std::endl;
        }

        file.close();
    }
}