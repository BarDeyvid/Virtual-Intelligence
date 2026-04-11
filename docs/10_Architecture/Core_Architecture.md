# Core Architecture

The Core Architecture of the Alyssa system is designed around a multi-agent "Expert Committee" and a central orchestration layer. Instead of a single monolithic model, the system utilizes specialized experts that contribute distinct perspectives (emotional, analytical, social, etc.) to a final fusion process.

## 1. Orchestration Layer (`CoreIntegration`)

The `CoreIntegration` class acts as the brain of the system, managing the lifecycle of an interaction. It is responsible for:
- **Input Processing**: Receiving user input and preparing it for the committee.
- **Expert Committee Execution**: Coordinating the parallel/sequential execution of specialized experts.
- **Weighted Fusion**: Aggregating thoughts from various experts into a unified prompt for the final model (Alyssa).
- **Context Management**: Managing KV cache, switching expert contexts, and preventing context leakage between turns.
- **Endocrine System Integration**: Updating the system's hormonal state based on interaction outcomes.

## 2. Expert Committee & Weighted Fusion

The system uses a "Mixture of Experts" (MoE) approach at the prompt level:
1. **Isolation**: Each expert runs in its own context, preventing interference between their specialized perspectives.
2. **Contribution**: Experts generate "thoughts" or specific viewpoints based on the input.
3. **Fusion**: The `generate_fused_input` method collects these thoughts and formats them into a structured prompt:
   - `[PENSAMENTOS]` block containing categorized expert outputs (e.g., `[Analítico]`, `[Emocional]`).
   - Integration of long-term memory context.
   - Injection of the current hormonal/endocrine state.

## 3. The Endocrine System

A unique feature of Alyssa is her "hormonal" regulation, which influences her personality and response style:
- **Metabolism**: Hormones gradually decay towards a baseline over time.
- **Feedback Loop**: Expert contributions can trigger updates to hormone levels (e.g., an analytical expert finding a contradiction might increase "stress/cortisol" signals).
- **System Prompt Injection**: The current hormonal profile is injected directly into the system prompt of the final fusion model.

## 4. Memory Management (LTM)

The `MemoryManager` handles Long-Term Memory (LTM) through:
- **Hybrid Retrieval**: Combining semantic search with heuristic-based retrieval.
- **Context Augmentation**: Relevant memories are injected into the "thoughts" block of the fused prompt.
- **Filtering**: To prevent context overflow, only the most relevant and recent memories are included in a single turn.

## 5. Data Flow Diagram (Conceptual)

`User Input` $\rightarrow$ `CoreIntegration` $\rightarrow$ `Expert Committee` $\rightarrow$ `Weighted Fusion + Memory + Endocrine` $\rightarrow$ `Alyssa (Final Model)` $\rightarrow$ `Output (Text/TTS)`
