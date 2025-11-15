// test_embedder.cpp
#include "Embedding/Embedder.hpp" // Use the correct path to your header
#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For std::setprecision
#include <stdexcept> // For std::exception

/**
 * @brief Helper function to print the first few dimensions of an embedding.
 */
void print_embedding(const std::vector<float>& emb, int dims_to_show = 8) {
    if (emb.empty()) {
        std::cout << "[ empty embedding ]" << std::endl;
        return;
    }

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "[ ";
    int n = std::min(static_cast<int>(emb.size()), dims_to_show);
    for (int i = 0; i < n; ++i) {
        std::cout << emb[i] << (i == n - 1 ? "" : ", ");
    }
    if (static_cast<int>(emb.size()) > n) {
        std::cout << ", ... ";
    }
    std::cout << "] (Dim: " << emb.size() << ")" << std::endl;
}

int main() {
    // ---
    // ⚠️ PREREQUISITE ⚠️
    // This test expects:
    // 1. A valid 'config/embedder_config.json' file.
    // 2. The 'model_path' inside that JSON points to a valid GGUF embedding model
    //    (one that supports sequence pooling, as checked in your initialize() function).
    // ---

    try {
        // --- 1. Initialization ---
        std::cout << "--- 1. Initializing Embedder ---" << std::endl;
        Embedder embedder("config/embedder_config.json");
        
        if (!embedder.initialize()) {
            std::cerr << "Fatal Error: Could not initialize Embedder." << std::endl;
            std::cerr << "Please check your 'config/embedder_config.json' and model path." << std::endl;
            return 1;
        }
        std::cout << "Embedder initialized successfully." << std::endl;
        std::cout << "Model embedding dimension: " << embedder.get_embedding_dimension() << std::endl;
        std::cout << "------------------------------------" << std::endl << std::endl;


        // --- 2. Test Single Embedding ---
        std::cout << "--- 2. Test: Single Embedding ---" << std::endl;
        std::string text1 = "The quick brown fox jumps over the lazy dog.";
        std::cout << "Input: \"" << text1 << "\"" << std::endl;
        
        std::vector<float> emb1 = embedder.generate_embedding(text1);
        std::cout << "Output: ";
        print_embedding(emb1);
        std::cout << "------------------------------------" << std::endl << std::endl;


        // --- 3. Test Batch Embedding ---
        std::cout << "--- 3. Test: Batch Embeddings ---" << std::endl;
        std::vector<std::string> texts = {
            "A fast animal.",
            "A slow creature.",
            "The weather is sunny today.",
            "What is the capital of France?"
        };
        
        std::vector<std::vector<float>> embeddings = embedder.generate_embeddings(texts);
        
        std::cout << "Batch size: " << texts.size() << ", Embeddings generated: " << embeddings.size() << std::endl;
        for (size_t i = 0; i < embeddings.size(); ++i) {
            std::cout << "Input " << i << ": \"" << texts[i] << "\"" << std::endl;
            std::cout << "Output " << i << ": ";
            print_embedding(embeddings[i]);
        }
        std::cout << "------------------------------------" << std::endl << std::endl;


        // --- 4. Test Cosine Similarity ---
        std::cout << "--- 4. Test: Cosine Similarity ---" << std::endl;
        if (embeddings.size() >= 2) {
            double sim_0_1 = Embedder::cosine_similarity(embeddings[0], embeddings[1]);
            std::cout << "Similarity (Input 0 vs Input 1): " << sim_0_1 << " (Should be low)" << std::endl;

            double sim_0_0 = Embedder::cosine_similarity(embeddings[0], embeddings[0]);
            std::cout << "Similarity (Input 0 vs Input 0): " << sim_0_0 << " (Should be ~1.0)" << std::endl;
        }
        std::cout << "------------------------------------" << std::endl << std::endl;


        // --- 5. Test Semantic Search ---
        std::cout << "--- 5. Test: Semantic Search ---" << std::endl;
        std::string query = "a speedy mammal";
        int top_k = 3;

        std::cout << "Query: \"" << query << "\"" << std::endl;
        std::cout << "Top " << top_k << " results from batch:" << std::endl;

        auto search_results = embedder.semantic_search(query, texts, top_k);
        
        for (const auto& res : search_results) {
            std::cout << "  - Score: " << std::setw(8) << std::get<0>(res) 
                      << " | Idx: " << std::get<1>(res) 
                      << " | Doc: \"" << std::get<2>(res) << "\"" << std::endl;
        }
        std::cout << "------------------------------------" << std::endl << std::endl;


    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "All tests completed successfully." << std::endl;
    return 0;
}