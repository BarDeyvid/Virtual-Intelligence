# class `alyssa_experts::ExpertBase`

 Base implementation of

## Detailed Description

 This class provides a concrete implementation of the

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`config`](#) |  Expert configuration. |
| std::vector< llama_chat_message > `variable `[`history`](#) |  Conversation history. |
| llama_adapter_lora * `variable `[`lora`](#) |  LoRA adapter (if used). |
| std::string `variable `[`expert_id`](#) |  Unique expert identifier. |
| std::string `function `[`parse_expert_signal`](#) |  Parse structured signal from expert's raw response. |
| `function `[`ExpertBase`](#) |  Constructor with configuration. |
| `function `[`~ExpertBase`](#) |  Destructor with resource cleanup. |
| const std::string & `function `[`get_id`](#) |  Get expert's unique identifier. |
| const `function `[`get_config`](#) |  Get expert's configuration. |
| const std::vector< llama_chat_message > & `function `[`get_history`](#) |  Get expert's conversation history. |
| void `function `[`clear_history`](#) |  Clear expert's conversation history. |
| bool `function `[`initialize`](#) |  Initialize the expert with a shared model. |
| std::string `function `[`run`](#) |  Main execution method for expert inference. |
| `function `[`get_contribution`](#) |  Get structured contribution for weighted fusion. |

## Members

### `config`

 Expert configuration.

---

### `history`

**Type**: std::vector< llama_chat_message >

 Conversation history.

---

### `lora`

**Type**: llama_adapter_lora *

 LoRA adapter (if used).

---

### `expert_id`

**Type**: std::string

 Unique expert identifier.

---

### `parse_expert_signal`

**Type**: std::string

 Parse structured signal from expert's raw response.

---

### `ExpertBase`

 Constructor with configuration.

---

### `~ExpertBase`

 Destructor with resource cleanup.

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

