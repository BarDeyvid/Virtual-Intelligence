# class `Embedder`

 Main class for generating text embeddings using llama.cpp models.

## Detailed Description

 This class provides functionality to load embedding models, generate embeddings for text inputs, compute similarity metrics, and perform semantic search. It supports various configuration options and batch processing for efficiency.

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`config`](#) |  Configuration settings. |
| bool `variable `[`initialized`](#) |  Initialization flag. |
| std::string `variable `[`config_path`](#) |  Path to config file. |
| std::shared_ptr< llama_model > `variable `[`model`](#) |  Shared pointer to llama model. |
| std::shared_ptr< llama_context > `variable `[`context`](#) |  Shared pointer to llama context. |
| const llama_vocab * `variable `[`vocab_ptr`](#) |  Pointer to model vocabulary. |
| llama_batch `variable `[`batch`](#) |  Batch for token processing. |
| int `variable `[`n_embd`](#) |  Embedding dimension from model. |
| enum llama_pooling_type `variable `[`pooling_type`](#) |  Pooling type for embeddings. |
| `function `[`Embedder`](#) |  Default constructor. |
| `function `[`Embedder`](#) |  Constructor with configuration file path. |
| `function `[`~Embedder`](#) |  Destructor. |
| bool `function `[`initialize`](#) |  Initialize the embedder with default config path. |
| bool `function `[`initialize`](#) |  Initialize the embedder with specified config file. |
| std::vector< float > `function `[`generate_embedding`](#) |  Generate embedding for a single text. |
| std::vector< std::vector< float > > `function `[`generate_embeddings`](#) |  Generate embeddings for multiple texts. |
| void `function `[`clear_embedding_cache`](#) |  Clear the embedding cache. |
| std::vector< std::tuple< double, int, std::string > > `function `[`semantic_search`](#) |  Perform semantic search on documents. |
| bool `function `[`is_initialized`](#) |  Check if embedder is initialized. |
| `function `[`get_config`](#) |  Get current configuration. |
| int `function `[`get_embedding_dimension`](#) |  Get embedding dimension. |
| double `function `[`cosine_similarity`](#) |  Compute cosine similarity between two embeddings. |
| double `function `[`dot_product`](#) |  Compute dot product of two embeddings. |
| double `function `[`magnitude`](#) |  Compute magnitude (L2 norm) of an embedding. |
| bool `function `[`config_load`](#) |  Load configuration from JSON file. |
| bool `function `[`save_config`](#) |  Save current configuration to JSON file. |
| void `function `[`create_default_config`](#) |  Create default configuration file. |
| std::vector< llama_token > `function `[`tokenize`](#) |  Tokenize text using model vocabulary. |
| void `function `[`batch_add_seq_internal`](#) |  Add sequence to batch for processing. |
| void `function `[`batch_decode_internal`](#) |  Decode batch and extract embeddings. |
| std::vector< float > `function `[`extract_embedding_from_json`](#) |  Extract embedding vector from JSON response. |
| size_t `function `[`write_callback`](#) |  Callback function for writing HTTP response data. |

## Members

### `config`

 Configuration settings.

---

### `initialized`

**Type**: bool

 Initialization flag.

---

### `config_path`

**Type**: std::string

 Path to config file.

---

### `model`

**Type**: std::shared_ptr< llama_model >

 Shared pointer to llama model.

---

### `context`

**Type**: std::shared_ptr< llama_context >

 Shared pointer to llama context.

---

### `vocab_ptr`

**Type**: const llama_vocab *

 Pointer to model vocabulary.

---

### `batch`

**Type**: llama_batch

 Batch for token processing.

---

### `n_embd`

**Type**: int

 Embedding dimension from model.

---

### `pooling_type`

**Type**: enum llama_pooling_type

 Pooling type for embeddings.

---

### `Embedder`

 Default constructor.

---

### `Embedder`

 Constructor with configuration file path.

---

### `~Embedder`

 Destructor.

---

### `initialize`

**Type**: bool

 Initialize the embedder with default config path.

---

### `initialize`

**Type**: bool

 Initialize the embedder with specified config file.

---

### `generate_embedding`

**Type**: std::vector< float >

 Generate embedding for a single text.

---

### `generate_embeddings`

**Type**: std::vector< std::vector< float > >

 Generate embeddings for multiple texts.

---

### `clear_embedding_cache`

**Type**: void

 Clear the embedding cache.

---

### `semantic_search`

**Type**: std::vector< std::tuple< double, int, std::string > >

 Perform semantic search on documents.

---

### `is_initialized`

**Type**: bool

 Check if embedder is initialized.

---

### `get_config`

 Get current configuration.

---

### `get_embedding_dimension`

**Type**: int

 Get embedding dimension.

---

### `cosine_similarity`

**Type**: double

 Compute cosine similarity between two embeddings.

---

### `dot_product`

**Type**: double

 Compute dot product of two embeddings.

---

### `magnitude`

**Type**: double

 Compute magnitude (L2 norm) of an embedding.

---

### `config_load`

**Type**: bool

 Load configuration from JSON file.

---

### `save_config`

**Type**: bool

 Save current configuration to JSON file.

---

### `create_default_config`

**Type**: void

 Create default configuration file.

---

### `tokenize`

**Type**: std::vector< llama_token >

 Tokenize text using model vocabulary.

---

### `batch_add_seq_internal`

**Type**: void

 Add sequence to batch for processing.

---

### `batch_decode_internal`

**Type**: void

 Decode batch and extract embeddings.

---

### `extract_embedding_from_json`

**Type**: std::vector< float >

 Extract embedding vector from JSON response.

---

### `write_callback`

**Type**: size_t

 Callback function for writing HTTP response data.

---

