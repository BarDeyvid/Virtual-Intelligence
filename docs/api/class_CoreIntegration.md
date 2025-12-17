# class `CoreIntegration`

 Main orchestration class for the Alyssa AI system using Mixture of Experts (MoE) architecture.

## Detailed Description

 This class integrates multiple specialized AI models (experts) with weighted fusion capabilities, memory management, and text-to-speech functionality. It serves as the central controller for processing user inputs through different expert models and generating coherent responses. Key features: Mixture of Experts (MoE) architecture with specialized models Weighted fusion of expert contributions Long-term memory integration Context switching between experts TTS (Text-to-Speech) integration Dynamic history management

## Summary

| Members | Descriptions |
|---------|--------------|
| std::unique_ptr< `variable `[`core_instance`](#) |  Base model instance (1B). |
| std::unique_ptr< `variable `[`fusion_engine`](#) |  Weighted fusion engine. |
| std::shared_ptr< `variable `[`embedder`](#) |  Embedding generator for semantic analysis. |
| std::unique_ptr< `variable `[`memory_manager`](#) |  Long-term memory manager. |
| std::unordered_map< std::string, std::unique_ptr< `variable `[`experts`](#) |  Map of registered expert models with their unique IDs. |
| std::unordered_map< std::string, std::vector< llama_chat_message > > `variable `[`expert_histories`](#) |  Conversation history for each expert. |
| std::string `variable `[`active_expert_in_cache`](#) |  Currently active expert in KV cache. |
| bool `variable `[`initialized`](#) |  System initialization status. |
| std::string `variable `[`user_name`](#) |  Current user's name for personalization. |
| `function `[`CoreIntegration`](#) |  Default constructor. |
| `function `[`~CoreIntegration`](#) |  Destructor. |
| void `function `[`set_user_name`](#) |  Set the user's name for personalized interactions. |
| bool `function `[`initialize`](#) |  Initialize the complete Alyssa AI system. |
| std::string `function `[`think`](#) |  Process user input using standard MoE architecture. |
| std::string `function `[`think_with_fusion`](#) |  Process user input with weighted fusion and TTS. |
| std::string `function `[`think_with_fusion_ttsless`](#) |  Process user input with weighted fusion without TTS. |
| void `function `[`register_expert`](#) |  Register a new expert model with the system. |
| void `function `[`remove_expert`](#) |  Remove an expert from the system. |
| bool `function `[`has_expert`](#) |  Check if an expert is registered. |
| bool `function `[`validate_context_size`](#) |  Validate if input fits within expert's context window. |
| std::string `function `[`detect_emotion_with_heuristics`](#) |  Detect emotional content in input using heuristics. |
| float `function `[`calculate_committee_coherence`](#) |  Calculate coherence metric for expert committee responses. |
| bool `function `[`should_store_in_memory`](#) |  Determine if interaction should be stored in long-term memory. |
| std::string `function `[`generate_fused_input`](#) |  Generate fused input for Alyssa model from expert contributions. |
| void `function `[`log_source_awareness`](#) |  Log source awareness information. |
| void `function `[`clear_all_cache`](#) |  Clear all caches including KV cache and expert histories. |
| void `function `[`run_interactive_loop`](#) |  Run interactive command-line interface. |
| std::string `function `[`run_expert`](#) |  Execute a specific expert with input processing. |
| bool `function `[`are_signals_compatible`](#) |  Check if two expert signals are compatible. |
| float `function `[`calculate_string_similarity`](#) |  Calculate string similarity using Jaccard index. |
| bool `function `[`is_small_talk`](#) |  Determine if input is small talk/social pleasantry. |
| std::vector< `function `[`run_expert_committee`](#) |  Run committee of experts in parallel. |
| void `function `[`manage_dynamic_history`](#) |  Manage expert history with dynamic size limits. |
| size_t `function `[`calculate_history_limit`](#) |  Calculate dynamic history limit based on expert and emotional state. |
| void `function `[`clear_kv_cache`](#) |  Clear KV cache for current expert. |
| void `function `[`switch_expert_context`](#) |  Switch active expert context in KV cache. |
| void `function `[`act`](#) |  Execute an action command. |
| void `function `[`reflect`](#) |  Perform system reflection/self-analysis. |

## Members

### `core_instance`

**Type**: std::unique_ptr<

 Base model instance (1B).

---

### `fusion_engine`

**Type**: std::unique_ptr<

 Weighted fusion engine.

---

### `embedder`

**Type**: std::shared_ptr<

 Embedding generator for semantic analysis.

---

### `memory_manager`

**Type**: std::unique_ptr<

 Long-term memory manager.

---

### `experts`

**Type**: std::unordered_map< std::string, std::unique_ptr<

 Map of registered expert models with their unique IDs.

---

### `expert_histories`

**Type**: std::unordered_map< std::string, std::vector< llama_chat_message > >

 Conversation history for each expert.

---

### `active_expert_in_cache`

**Type**: std::string

 Currently active expert in KV cache.

---

### `initialized`

**Type**: bool

 System initialization status.

---

### `user_name`

**Type**: std::string

 Current user's name for personalization.

---

### `CoreIntegration`

 Default constructor.

---

### `~CoreIntegration`

 Destructor.

---

### `set_user_name`

**Type**: void

 Set the user's name for personalized interactions.

---

### `initialize`

**Type**: bool

 Initialize the complete Alyssa AI system.

---

### `think`

**Type**: std::string

 Process user input using standard MoE architecture.

---

### `think_with_fusion`

**Type**: std::string

 Process user input with weighted fusion and TTS.

---

### `think_with_fusion_ttsless`

**Type**: std::string

 Process user input with weighted fusion without TTS.

---

### `register_expert`

**Type**: void

 Register a new expert model with the system.

---

### `remove_expert`

**Type**: void

 Remove an expert from the system.

---

### `has_expert`

**Type**: bool

 Check if an expert is registered.

---

### `validate_context_size`

**Type**: bool

 Validate if input fits within expert's context window.

---

### `detect_emotion_with_heuristics`

**Type**: std::string

 Detect emotional content in input using heuristics.

---

### `calculate_committee_coherence`

**Type**: float

 Calculate coherence metric for expert committee responses.

---

### `should_store_in_memory`

**Type**: bool

 Determine if interaction should be stored in long-term memory.

---

### `generate_fused_input`

**Type**: std::string

 Generate fused input for Alyssa model from expert contributions.

---

### `log_source_awareness`

**Type**: void

 Log source awareness information.

---

### `clear_all_cache`

**Type**: void

 Clear all caches including KV cache and expert histories.

---

### `run_interactive_loop`

**Type**: void

 Run interactive command-line interface.

---

### `run_expert`

**Type**: std::string

 Execute a specific expert with input processing.

---

### `are_signals_compatible`

**Type**: bool

 Check if two expert signals are compatible.

---

### `calculate_string_similarity`

**Type**: float

 Calculate string similarity using Jaccard index.

---

### `is_small_talk`

**Type**: bool

 Determine if input is small talk/social pleasantry.

---

### `run_expert_committee`

**Type**: std::vector<

 Run committee of experts in parallel.

---

### `manage_dynamic_history`

**Type**: void

 Manage expert history with dynamic size limits.

---

### `calculate_history_limit`

**Type**: size_t

 Calculate dynamic history limit based on expert and emotional state.

---

### `clear_kv_cache`

**Type**: void

 Clear KV cache for current expert.

---

### `switch_expert_context`

**Type**: void

 Switch active expert context in KV cache.

---

### `act`

**Type**: void

 Execute an action command.

---

### `reflect`

**Type**: void

 Perform system reflection/self-analysis.

---

