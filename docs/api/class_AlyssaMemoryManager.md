# class `alyssa_memory::AlyssaMemoryManager`

 High-level manager for conversational AI memory integration.

## Detailed Description

 Provides simplified interface for integrating memory system with conversational AI, handling emotional analysis, intention management, and memory retrieval.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::unique_ptr< `variable `[`memory_system`](#) |  Underlying memory system. |
| `variable `[`search_config_`](#) |  Search configuration. |
| void `function `[`printEmotionalAnalysis`](#) |  Print emotional analysis results to console. |
| void `function `[`analyzeInputForIntentions`](#) |  Analyze input text for intention triggers. |
| void `function `[`setSearchConfig`](#) |  Set search configuration. |
| const `function `[`getSearchConfig`](#) |  Get current search configuration. |
| `function `[`AlyssaMemoryManager`](#) |  Constructor for |
| void `function `[`processInteraction`](#) |  Process interaction with automatic emotional analysis. |
| void `function `[`processInteraction`](#) |  Process interaction with provided emotional vector (legacy). |
| std::vector< `function `[`getRelevantMemories`](#) |  Get relevant memories for context. |
| std::vector< `function `[`getSemanticMemories`](#) |  Get semantic memories for context. |
| std::vector< `function `[`getHybridMemories`](#) |  Get hybrid search results for context. |
| `function `[`getCurrentEmotionalState`](#) |  Get current emotional state. |
| std::vector< `function `[`getActiveIntentions`](#) |  Get active intentions. |
| int `function `[`storeMemoryWithEmotionalAnalysis`](#) |  Store memory with emotional analysis. |
| void `function `[`linkMemoryToIntention`](#) |  Link memory to intention. |
| void `function `[`setCurrentGoal`](#) |  Set current goal/intention. |
| void `function `[`processIdentityFact`](#) |  Process identity fact for user modeling. |

## Members

### `memory_system`

**Type**: std::unique_ptr<

 Underlying memory system.

---

### `search_config_`

 Search configuration.

---

### `printEmotionalAnalysis`

**Type**: void

 Print emotional analysis results to console.

---

### `analyzeInputForIntentions`

**Type**: void

 Analyze input text for intention triggers.

---

### `setSearchConfig`

**Type**: void

 Set search configuration.

---

### `getSearchConfig`

**Type**: const

 Get current search configuration.

---

### `AlyssaMemoryManager`

 Constructor for

---

### `processInteraction`

**Type**: void

 Process interaction with automatic emotional analysis.

---

### `processInteraction`

**Type**: void

 Process interaction with provided emotional vector (legacy).

---

### `getRelevantMemories`

**Type**: std::vector<

 Get relevant memories for context.

---

### `getSemanticMemories`

**Type**: std::vector<

 Get semantic memories for context.

---

### `getHybridMemories`

**Type**: std::vector<

 Get hybrid search results for context.

---

### `getCurrentEmotionalState`

 Get current emotional state.

---

### `getActiveIntentions`

**Type**: std::vector<

 Get active intentions.

---

### `storeMemoryWithEmotionalAnalysis`

**Type**: int

 Store memory with emotional analysis.

---

### `linkMemoryToIntention`

**Type**: void

 Link memory to intention.

---

### `setCurrentGoal`

**Type**: void

 Set current goal/intention.

---

### `processIdentityFact`

**Type**: void

 Process identity fact for user modeling.

---

