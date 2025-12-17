# class `alyssa_fusion::WeightedFusion`

 A class that performs weighted fusion of expert responses based on various criteria.

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`embedder`](#) |  |
| Ort::Env `variable `[`env`](#) |  |
| Ort::Session `variable `[`session`](#) |  |
| double `variable `[`emotion_weight_base`](#) |  Base weight for emotional context. |
| double `variable `[`context_similarity_weight`](#) |  Weight for similarity in context. |
| double `variable `[`historical_relevance_weight`](#) |  Weight for historical relevance. |
| std::map< std::string, double > `variable `[`expert_affinities`](#) |  |
| `function `[`WeightedFusion`](#) |  Constructor for the |
| std::map< std::string, double > `function `[`calculate_rule_based_weights`](#) |  Calculates rule-based weights for expert contributions. |
| std::map< std::string, double > `function `[`calculate_feature_based_weights`](#) |  Calculates feature-based weights for expert contributions. |
| std::map< std::string, double > `function `[`calculate_neural_weights`](#) |  Calculates neural-based weights for expert contributions using an ONNX model. |
| std::string `function `[`fuse_responses`](#) |  Fuses multiple expert responses into a single output. |
| double `function `[`calculate_semantic_similarity`](#) |  Calculates the semantic similarity between two embeddings. |
| std::string `function `[`detect_emotion_from_input`](#) |  Detects the emotion from the user's input. |
| std::string `function `[`extract_keywords`](#) |  Extracts keywords from the given text. |

## Members

### `embedder`

---

### `env`

**Type**: Ort::Env

---

### `session`

**Type**: Ort::Session

---

### `emotion_weight_base`

**Type**: double

 Base weight for emotional context.

---

### `context_similarity_weight`

**Type**: double

 Weight for similarity in context.

---

### `historical_relevance_weight`

**Type**: double

 Weight for historical relevance.

---

### `expert_affinities`

**Type**: std::map< std::string, double >

---

### `WeightedFusion`

 Constructor for the

---

### `calculate_rule_based_weights`

**Type**: std::map< std::string, double >

 Calculates rule-based weights for expert contributions.

---

### `calculate_feature_based_weights`

**Type**: std::map< std::string, double >

 Calculates feature-based weights for expert contributions.

---

### `calculate_neural_weights`

**Type**: std::map< std::string, double >

 Calculates neural-based weights for expert contributions using an ONNX model.

---

### `fuse_responses`

**Type**: std::string

 Fuses multiple expert responses into a single output.

---

### `calculate_semantic_similarity`

**Type**: double

 Calculates the semantic similarity between two embeddings.

---

### `detect_emotion_from_input`

**Type**: std::string

 Detects the emotion from the user's input.

---

### `extract_keywords`

**Type**: std::string

 Extracts keywords from the given text.

---

