// EmotionLLM.hpp
#pragma once
#include "llama.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

class EmotionLLM {
    private:
    // Shared Pointers, No need to delete them
    llama_model* model;
    const llama_vocab* vocab;

    // Own Resource, delete 'em
    llama_context* ctx;
    llama_sampler* smpl;

    std::string system_prompt;
    std::vector<llama_chat_message> history; // Guarantees that it's on the class
    int n_ctx = 2048; //TODO: Change it from static to dynamic
    
    public:
        EmotionLLM(llama_model* shared_model, const llama_vocab* shared_vocab, const std::string& lora_adapter_path)
            : model(shared_model), vocab(shared_vocab), ctx(nullptr), smpl(nullptr)
        {
            if (!model && !vocab) {

                // 1. Create inference context
                llama_context_params ctx_params = llama_context_default_params();
                ctx_params.n_ctx = n_ctx;
                ctx_params.n_batch = n_ctx;
                
                ctx = llama_init_from_model(model, ctx_params);
                if (!ctx) {
                    throw std::runtime_error("EmotionLLM: Falha ao criar contexto.");
                }
                std::cout << "Contexto [EmotionLLM] criado." << std::endl;

                // 2. Apply Low Rank Adaptation former LoRA
                if (!lora_adapter_path.empty()) {
                    int err = llama_model_apply_lora_from_file(
                        ctx, // Apply our specific context
                        lora_adapter_path.c_str(),
                        NULL, // scale (NULL = default 1.0)
                        NULL, // base model (Already set)
                        4 // n_threads
                    );
                    if (err != 0) {
                        llama_free(ctx);
                        throw std::runtime_error("EmotionLLM: Lora failed: " + lora_adapter_path);
                    }
                    std::cout << "Lora Adapter [EmotionLLM] applied: " << lora_adapter_path << std::endl;
                    
                    // 3. Create the Amostrator
                    smpl = llama_sampler_chain_init(llama_sample_chain_default_params());
                    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1));
                    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f)); // Specific Temp of the Emotional
                    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
                }
                
            }
            // Modified Destructor
            ~EmotionLLM() {
                if (smpl) llama_sampler_free(smpl);
                if (ctx) llama_free(ctx);

                for (auto&msg : history) {
                    free((char*)msg.content);
                }
                std::cout << "Recursos [EmotionLLM liberados]"
            }

            void set_system_prompt(const std::string& prompt) {
                system_prompt = prompt;
            }

            std::string generate_raw(const std::string & prompt) {
                std::string response;

                // is first exec?
                const bool is_first = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) == -1;

                // Tokenize the thing
                const int n_prompt_tokens = -llama_tokenize(
                    vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first, true
                );

                std::vector<llama_token> prompt_tokens(n_prompt_tokens);
                if (llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                                prompt_tokens.data(), prompt_tokens.size(),
                                is_first, true) < 0) {
                    throw std::runtime_error("Falha ao tokenizar o prompt\n");
                }

                // New batch with the tokens
                llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size())
                llama_token new_token_id;

                // Main Generation loop
                while (true) {
                    // TODO: Make this dynamic with base ctx
                    int n_ctx_total = llama_n_ctx(ctx);
                    int n_ctx_used = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;
                    if (n_ctx_used + batch.n_tokens > n_ctx_total) {
                        printf("\033[0m\n");
                        fprintf(stderr, "Tamanho do contexto excedido\n");

                        throw std::runtime_error("Tamanho do contexto excedido\n");
                    }

                    // Decode the actual tokens
                    int ret = llama_decode(ctx, batch);
                    if (ret != 0) throw std::runtime_error("Falha ao decodificar");

                    // Amostrate the next token
                    new_token_id = llama_sampler_sample(smpl, ctx, -1);

                    // Verify the end of the generation
                    if (llama_vocab_is_eog(vocab, new_token_id)) break;

                    // Token --> Text
                    char buf[256];
                    int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                    if (n < 0) throw std::runtime_error("Falha ao converter token\n");

                    std::string piece(buf, n);
                    printf("%s", piece.c_str());
                    fflush(stdout);
                    response += piece;

                    // Prepare the next batch
                    batch = llama_batch_get_one(&new_token_id, 1);
                }

                return response;
            }
            
            // Main Method: --> Input --> Output
            std::string generate_response(const std::string & user_input) {
                
                // 1. User Input to history
                this->history.push_back({"user", strdup(user_input.c_str())});

                // 2. Full list of messages (System + history)
                std::vector<llama_chat_message> messages_to_template;

                // Add the System Prompt in the start
                if (!system_prompt.empty()) {
                messages_to_template.push_back({"system", strdup(system_prompt.c_str())}); 
                // NOTE: This memory needs to be exitted (strdup)
            }
            
            // Add the history on the convo
            messages_to_template.insert(messages_to_template.end(), this->history.begin(), this->history.end());

            // 3. Apply the template and generate the prompt
            std::vector<char> formatted(llama_n_ctx(ctx));
            const char * tmpl = llama_model_chat_template(model, nullptr);

            int len = llama_chat_apply_template(
                tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
            );
            
            if (!system_prompt.empty()) {
                free((char*)messages_to_template[0].content);
            }

            if (len < 0 || len > (int)formatted.size()) {
                 if (len > (int)formatted.size()) {
                    formatted.resize(len);
                    len = llama_chat_apply_template(
                        tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
                    );
                 }
                 if (len < 0) {
                     fprintf(stderr, "Falha ao aplicar template de chat\n");
                     return "Erro ao processar a conversa.";
                 }
            }
            
            std::string prompt(formatted.begin(), formatted.begin() + len);
            
            // 4. Calls low level generation
            std::string response = generate_raw(prompt); 

            // 5. Add Alyssa's Response to the history
            this->history.push_back({"assistant", strdup(response.c_str())});

            // 6. Returns the answer
            return response;
        }
};