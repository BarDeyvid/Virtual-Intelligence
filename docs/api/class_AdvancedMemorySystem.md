# class `alyssa_memory::AdvancedMemorySystem`

 Advanced memory system with vector embeddings and emotional analysis.

## Detailed Description

 Core system for storing, retrieving, and analyzing memories with emotional context and semantic search capabilities.

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`db`](#) |  SQLite database connection. |
| `variable `[`current_emotional_state`](#) |  Current emotional state. |
| std::vector< `variable `[`active_intentions`](#) |  Active intentions. |
| std::map< std::string, double > `variable `[`emotion_weights`](#) |  Emotion weighting factors. |
| std::shared_ptr< `variable `[`embedder`](#) |  Embedding generator. |
| std::unique_ptr< `variable `[`emotional_analyzer`](#) |  Emotional analyzer. |
| std::unordered_map< std::string, std::pair< std::string, std::string > > `variable `[`emotion_to_intention`](#) |  Mapping from emotions to corresponding intentions. |
| void `function `[`initializeDatabase`](#) |  Initialize database tables. |
| void `function `[`loadCurrentState`](#) |  Load current emotional state from database. |
| void `function `[`saveCurrentState`](#) |  Save current emotional state to database. |
| void `function `[`loadActiveIntentions`](#) |  Load active intentions from database. |
| void `function `[`storeEmbedding`](#) |  Store embedding vector for memory. |
| void `function `[`checkEmotionalAutoActivation`](#) |  Check for automatic intention activation based on emotions. |
| void `function `[`saveReflection`](#) |  Save reflection to database. |
| double `function `[`calculateIntentionBoost`](#) |  Calculate intention boost for memory relevance. |
| double `function `[`calculateEmotionalBoost`](#) |  Calculate emotional boost for memory relevance. |
| double `function `[`calculateAutomaticImportance`](#) |  Calculate automatic importance score for memory. |
| `function `[`AdvancedMemorySystem`](#) |  Constructor for |
| `function `[`~AdvancedMemorySystem`](#) |  Destructor - saves state and closes database. |
| void `function `[`setEmotionalState`](#) |  Set current emotional state. |
| `function `[`getCurrentEmotionalState`](#) |  Get current emotional state. |
| void `function `[`linkMemoryToIntention`](#) |  Link memory to intention. |
| int `function `[`storeMemoryWithEmotionalAnalysis`](#) |  Store memory with automatic emotional analysis. |
| void `function `[`applyMemoryDecay`](#) |  Apply memory decay over time. |
| void `function `[`activateIntention`](#) |  Activate new intention. |
| void `function `[`deactivateIntention`](#) |  Deactivate intention by ID. |
| std::vector< `function `[`getActiveIntentions`](#) |  Get active intentions. |
| void `function `[`generateReflections`](#) |  Generate reflections based on recent memories. |
| void `function `[`createMemoryLink`](#) |  Create link between memories. |
| std::vector< `function `[`getMemoryLinks`](#) |  Get links associated with memory. |
| std::vector< `function `[`searchContextualMemories`](#) |  Search memories contextually (text-based). |
| std::vector< `function `[`semanticSearch`](#) |  Search memories semantically (embedding-based). |
| std::vector< `function `[`semanticSearchWithEmbedding`](#) |  Search memories with pre-computed embedding. |
| std::vector< `function `[`hybridSearch`](#) |  Hybrid search (text + semantic). |
| bool `function `[`hasEmbedder`](#) |  Check if embedder is available. |
| bool `function `[`generateAndStoreEmbedding`](#) |  Generate and store embedding for memory. |
| `function `[`analyzeEmotionalContent`](#) |  Analyze emotional content of text. |
| `function `[`analyzeConversationEmotions`](#) |  Analyze emotions in conversation. |
| std::vector< float > `function `[`getAutoEmotionalVector`](#) |  Get automatic emotional vector for text. |
| void `function `[`printSystemStatus`](#) |  Print system status to console. |
| void `function `[`demonstrateSemanticSearch`](#) |  Demonstrate semantic search with debug output. |

## Members

### `db`

 SQLite database connection.

---

### `current_emotional_state`

 Current emotional state.

---

### `active_intentions`

**Type**: std::vector<

 Active intentions.

---

### `emotion_weights`

**Type**: std::map< std::string, double >

 Emotion weighting factors.

---

### `embedder`

**Type**: std::shared_ptr<

 Embedding generator.

---

### `emotional_analyzer`

**Type**: std::unique_ptr<

 Emotional analyzer.

---

### `emotion_to_intention`

**Type**: std::unordered_map< std::string, std::pair< std::string, std::string > >

 Mapping from emotions to corresponding intentions.

---

### `initializeDatabase`

**Type**: void

 Initialize database tables.

---

### `loadCurrentState`

**Type**: void

 Load current emotional state from database.

---

### `saveCurrentState`

**Type**: void

 Save current emotional state to database.

---

### `loadActiveIntentions`

**Type**: void

 Load active intentions from database.

---

### `storeEmbedding`

**Type**: void

 Store embedding vector for memory.

---

### `checkEmotionalAutoActivation`

**Type**: void

 Check for automatic intention activation based on emotions.

---

### `saveReflection`

**Type**: void

 Save reflection to database.

---

### `calculateIntentionBoost`

**Type**: double

 Calculate intention boost for memory relevance.

---

### `calculateEmotionalBoost`

**Type**: double

 Calculate emotional boost for memory relevance.

---

### `calculateAutomaticImportance`

**Type**: double

 Calculate automatic importance score for memory.

---

### `AdvancedMemorySystem`

 Constructor for

---

### `~AdvancedMemorySystem`

 Destructor - saves state and closes database.

---

### `setEmotionalState`

**Type**: void

 Set current emotional state.

---

### `getCurrentEmotionalState`

 Get current emotional state.

---

### `linkMemoryToIntention`

**Type**: void

 Link memory to intention.

---

### `storeMemoryWithEmotionalAnalysis`

**Type**: int

 Store memory with automatic emotional analysis.

---

### `applyMemoryDecay`

**Type**: void

 Apply memory decay over time.

---

### `activateIntention`

**Type**: void

 Activate new intention.

---

### `deactivateIntention`

**Type**: void

 Deactivate intention by ID.

---

### `getActiveIntentions`

**Type**: std::vector<

 Get active intentions.

---

### `generateReflections`

**Type**: void

 Generate reflections based on recent memories.

---

### `createMemoryLink`

**Type**: void

 Create link between memories.

---

### `getMemoryLinks`

**Type**: std::vector<

 Get links associated with memory.

---

### `searchContextualMemories`

**Type**: std::vector<

 Search memories contextually (text-based).

---

### `semanticSearch`

**Type**: std::vector<

 Search memories semantically (embedding-based).

---

### `semanticSearchWithEmbedding`

**Type**: std::vector<

 Search memories with pre-computed embedding.

---

### `hybridSearch`

**Type**: std::vector<

 Hybrid search (text + semantic).

---

### `hasEmbedder`

**Type**: bool

 Check if embedder is available.

---

### `generateAndStoreEmbedding`

**Type**: bool

 Generate and store embedding for memory.

---

### `analyzeEmotionalContent`

 Analyze emotional content of text.

---

### `analyzeConversationEmotions`

 Analyze emotions in conversation.

---

### `getAutoEmotionalVector`

**Type**: std::vector< float >

 Get automatic emotional vector for text.

---

### `printSystemStatus`

**Type**: void

 Print system status to console.

---

### `demonstrateSemanticSearch`

**Type**: void

 Demonstrate semantic search with debug output.

---

