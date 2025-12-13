// EmotionLexiconLoader.hpp 
#pragma once

#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "json.hpp" 

using json = nlohmann::json;

namespace alyssa_memory {
    class EmotionLexiconLoader {
    public:
        static const std::string DEFAULT_LEXICON_PATH;
        
        static std::unordered_map<std::string, std::vector<std::string>> loadDefaultPortugueseLexicon();
        
        static std::unordered_map<std::string, std::vector<std::string>> loadLexiconFromFile(const std::string& filepath);
        
        static void saveLexiconToFile(
            const std::unordered_map<std::string, std::vector<std::string>>& lexicon,
            const std::string& filepath);
    };
}