# Agent Implementation Guide

This document outlines the standards and architectural patterns for implementing new agents (Experts) within the Alyssa ecosystem. Every agent added to the system must follow the "Expert Committee" pattern to ensure seamless integration with the `CoreIntegration` orchestration layer.

## 1. Core Responsibility

An agent is a specialized "thought generator." It should not attempt to act as the final persona (`alyssa`), but rather provide a specific, high-quality perspective (e.g., analytical, emotional, social, or creative) that the fusion engine can use to enrich the final response.

## 2. Architectural Pattern: The Specialist Role

Each agent follows the **Isolated Context** pattern:
1.  **Input**: Receives the current user input and a truncated history of recent interactions (to prevent context leakage).
2.  **Processing**: Performs specialized reasoning, retrieval, or heuristic analysis.
3.  **Output**: Produces a concise "thought" or "contribution" in a format that can be categorized by type.

## 3. Implementation Requirements

### A. Configuration (`ConfigsLLM.json`)
Every agent must be registered in the configuration file. The `id` used here determines:
-   The expert's identity during execution.
-   The category label used in the fused `[PENSAMENTOS]` block (e.g., `analyt_expert` $\rightarrow$ `[Analítico]`).

### B. Context Isolation
Agents must operate as if they are in a "fresh" session. While they have access to relevant history, their internal logic should not be influenced by the output of other experts from the *same* committee turn. This prevents "echo chamber" effects where one expert simply repeats another.

### C. Contribution Format
Contributions should ideally be structured as text blocks that fit within a larger prompt template. Avoid long-winded introductions; focus on the core insight or observation.

## 4. Agent Lifecycle in a Turn

When `CoreIntegration::run_expert_committee` is called, the following happens for each agent:
1.  **Context Switch**: The system switches the LLM context/LoRA to the specialist's configuration.
2.  **Execution**: The agent processes the `augmented_input`.
3.  **Extraction**: The response is captured as an `ExpertContribution`.
4.  **Cleanup**: The KV cache is cleared, and the system prepares for the next expert or the final fusion.

## 5. Best Practices

-   **Avoid Redundancy**: If you are building a "Social" agent, do not repeat what the "Emotional" agent would say. Focus strictly on social dynamics, etiquette, and relationship context.
-   **Heuristic Fallbacks**: If an agent's specialized tool (like a database lookup) fails, it should return a neutral, informative fallback rather than an error string that could break the fusion prompt.
-   **Token Efficiency**: Keep contributions concise. The final `alyssa` model has a finite context window; overly verbose experts will displace important memory or endocrine data.
-   **Deterministic Logic**: Where possible, use deterministic heuristics (like the `detect_emotion_with_heuristics` pattern) to augment LLM-based reasoning.

## 6. Summary of Agent Types

| Type | Focus Area | Example Contribution |
| :--- | :--- | :--- |
| **Analytic** | Logic, facts, contradictions, structure. | "The user's claim contradicts the previous memory regarding..." |
| **Emotional** | Sentiment, empathy, tone matching. | "The user seems frustrated; a soft approach is needed." |
| **Social** | Etiquette, interpersonal dynamics, warmth. | "Acknowledge the user's greeting with appropriate politeness." |
| **Creative** | Divergent thinking, metaphors, storytelling. | "Imagine this scenario as if it were a scene from a noir film..." |
| **Memory** | Retrieval and synthesis of past interactions. | "The user previously mentioned they dislike spicy food." |
