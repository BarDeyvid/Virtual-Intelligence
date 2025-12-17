# class `alyssa_core::AlyssaCore`

 Class to manage language models and generate text.

## Detailed Description

 The

## Summary

| Members | Descriptions |
|---------|--------------|
| llama_model * `variable `[`model`](#) |  Pointer to the loaded base model. |
| const llama_vocab * `variable `[`vocab`](#) |  Pointer to the vocabulary used by the model. |
| llama_model_params `variable `[`mParams`](#) |  Parameters for the model loading. |
| llama_context * `variable `[`ctx`](#) |  Pointer to the unique shared context. |
| int `variable `[`n_ctx`](#) |  Size of the context. |
| `function `[`AlyssaCore`](#) |  Constructor for |
| `function `[`~AlyssaCore`](#) |  Destructor for |
| llama_model * `function `[`get_model`](#) |  Getter for the model. |
| const llama_vocab * `function `[`get_vocab`](#) |  Getter for the vocabulary. |
| llama_context * `function `[`get_context`](#) |  Getter for the context. |
| int `function `[`get_n_ctx`](#) |  Getter for the context size. |
| std::string `function `[`generate_raw`](#) |  Generates text based on a given prompt and parameters. |

## Members

### `model`

**Type**: llama_model *

 Pointer to the loaded base model.

---

### `vocab`

**Type**: const llama_vocab *

 Pointer to the vocabulary used by the model.

---

### `mParams`

**Type**: llama_model_params

 Parameters for the model loading.

---

### `ctx`

**Type**: llama_context *

 Pointer to the unique shared context.

---

### `n_ctx`

**Type**: int

 Size of the context.

---

### `AlyssaCore`

 Constructor for

---

### `~AlyssaCore`

 Destructor for

---

### `get_model`

**Type**: llama_model *

 Getter for the model.

---

### `get_vocab`

**Type**: const llama_vocab *

 Getter for the vocabulary.

---

### `get_context`

**Type**: llama_context *

 Getter for the context.

---

### `get_n_ctx`

**Type**: int

 Getter for the context size.

---

### `generate_raw`

**Type**: std::string

 Generates text based on a given prompt and parameters.

---

