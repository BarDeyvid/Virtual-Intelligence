# struct `Embedder::Config`

 Configuration structure for

## Summary

| Members | Descriptions |
|---------|--------------|
| std::string `variable `[`model_path`](#) |  Path to the model file. |
| int `variable `[`embedding_dimension`](#) |  Dimension of output embeddings. |
| int32_t `variable `[`seed`](#) |  Random seed (-1 for random). |
| int32_t `variable `[`n_ctx`](#) |  Context window size. |
| int32_t `variable `[`n_batch`](#) |  Batch size for processing. |
| int32_t `variable `[`n_threads`](#) |  Number of threads (0 = auto). |
| int32_t `variable `[`n_gpu_layers`](#) |  Number of GPU layers to use. |
| bool `variable `[`use_mlock`](#) |  Lock model in memory. |
| bool `variable `[`use_mmap`](#) |  Use memory mapping. |
| bool `variable `[`f16_kv`](#) |  Use FP16 for KV cache. |
| int `variable `[`embd_normalize`](#) |  Embedding normalization mode (0=none, 1=L2, 2=scaled). |

## Members

### `model_path`

**Type**: std::string

 Path to the model file.

---

### `embedding_dimension`

**Type**: int

 Dimension of output embeddings.

---

### `seed`

**Type**: int32_t

 Random seed (-1 for random).

---

### `n_ctx`

**Type**: int32_t

 Context window size.

---

### `n_batch`

**Type**: int32_t

 Batch size for processing.

---

### `n_threads`

**Type**: int32_t

 Number of threads (0 = auto).

---

### `n_gpu_layers`

**Type**: int32_t

 Number of GPU layers to use.

---

### `use_mlock`

**Type**: bool

 Lock model in memory.

---

### `use_mmap`

**Type**: bool

 Use memory mapping.

---

### `f16_kv`

**Type**: bool

 Use FP16 for KV cache.

---

### `embd_normalize`

**Type**: int

 Embedding normalization mode (0=none, 1=L2, 2=scaled).

---

