# class `alyssa_experts::IExpert`

 Interface for specialized expert models in the Mixture of Experts architecture.

## Detailed Description

 This interface defines the contract that all expert models must implement. Experts are specialized AI models focused on specific domains (emotional analysis, memory retrieval, creative thinking, etc.) that contribute to the overall system.

## Summary

| Members | Descriptions |
|---------|--------------|
| `function `[`~IExpert`](#) |  Virtual destructor for proper cleanup. |
| bool `function `[`initialize`](#) |  Initialize the expert with a shared model. |
| std::string `function `[`run`](#) |  Main execution method for expert inference. |
| `function `[`get_contribution`](#) |  Get structured contribution for weighted fusion. |
| const std::string & `function `[`get_id`](#) |  Get expert's unique identifier. |
| const `function `[`get_config`](#) |  Get expert's configuration. |
| const std::vector< llama_chat_message > & `function `[`get_history`](#) |  Get expert's conversation history. |
| void `function `[`clear_history`](#) |  Clear expert's conversation history. |

## Members

### `~IExpert`

 Virtual destructor for proper cleanup.

---

### `initialize`

**Type**: bool

 Initialize the expert with a shared model.

---

### `run`

**Type**: std::string

 Main execution method for expert inference.

---

### `get_contribution`

 Get structured contribution for weighted fusion.

---

### `get_id`

**Type**: const std::string &

 Get expert's unique identifier.

---

### `get_config`

**Type**: const

 Get expert's configuration.

---

### `get_history`

**Type**: const std::vector< llama_chat_message > &

 Get expert's conversation history.

---

### `clear_history`

**Type**: void

 Clear expert's conversation history.

---

