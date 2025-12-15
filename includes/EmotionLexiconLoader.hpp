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
    /**
     * @class EmotionLexiconLoader
     * @brief Handles loading and saving of emotion lexicons from/to JSON files.
     * 
     * This class provides static methods to load emotion lexicons from JSON files,
     * save lexicons to files, and access the default Portuguese lexicon.
     * Lexicons are stored as a map where emotions are keys and word lists are values.
     */
    class EmotionLexiconLoader {
    public:
        static const std::string DEFAULT_LEXICON_PATH;
        
        /**
         * @brief Loads the default Portuguese lexicon from the default file path.
         * @return std::unordered_map<std::string, std::vector<std::string>> 
         *         A map containing emotions as keys and vectors of associated words as values.
         *         Returns an empty map if loading fails.
         */
        static std::unordered_map<std::string, std::vector<std::string>> loadDefaultPortugueseLexicon();
        
        /**
         * @brief Loads an emotion lexicon from a specified JSON file.
         * @param filepath Path to the JSON lexicon file.
         * @return std::unordered_map<std::string, std::vector<std::string>> 
         *         A map containing emotions as keys and vectors of associated words as values.
         *         Returns an empty map if the file cannot be opened or parsed.
         */
        static std::unordered_map<std::string, std::vector<std::string>> loadLexiconFromFile(const std::string& filepath);
        
        /**
         * @brief Saves an emotion lexicon to a JSON file.
         * @param lexicon The lexicon map to save.
         * @param filepath Path where the JSON file will be created/overwritten.
         */
        static void saveLexiconToFile(
            const std::unordered_map<std::string, std::vector<std::string>>& lexicon,
            const std::string& filepath);
    };
}