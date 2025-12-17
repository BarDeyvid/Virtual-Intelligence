# struct `SimpleModelConfig`

 Structure to hold configuration details for a simple model.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::string `variable `[`id`](#) |  Unique identifier for the model. |
| std::string `variable `[`model_path`](#) |  Path to the base model file. |
| std::string `variable `[`system_prompt`](#) |  System prompt for initializing the model context. |
| std::string `variable `[`role_instruction`](#) |  Instruction or role assigned to the model. |
| bool `variable `[`usa_LoRA`](#) |  Flag indicating if LoRA adaptation should be used. |
| std::string `variable `[`lora_path`](#) |  Path to the LoRA file (if applicable). |
| `variable `[`params`](#) |  Parameters for model generation. |
| int `variable `[`n_ctx`](#) |  Context size for the model. |

## Members

### `id`

**Type**: std::string

 Unique identifier for the model.

---

### `model_path`

**Type**: std::string

 Path to the base model file.

---

### `system_prompt`

**Type**: std::string

 System prompt for initializing the model context.

---

### `role_instruction`

**Type**: std::string

 Instruction or role assigned to the model.

---

### `usa_LoRA`

**Type**: bool

 Flag indicating if LoRA adaptation should be used.

---

### `lora_path`

**Type**: std::string

 Path to the LoRA file (if applicable).

---

### `params`

 Parameters for model generation.

---

### `n_ctx`

**Type**: int

 Context size for the model.

---

