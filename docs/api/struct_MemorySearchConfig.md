# struct `MemorySearchConfig`

 Configuration structure for memory search settings.

## Detailed Description

 Contains parameters for hybrid search (text + semantic) and result boosting.

## Summary

| Members | Descriptions |
|---------|--------------|
| int `variable `[`default_top_k`](#) |  Default number of top results to return. |
| double `variable `[`text_weight`](#) |  Weight for textual similarity in hybrid search. |
| double `variable `[`semantic_weight`](#) |  Weight for semantic similarity in hybrid search. |
| double `variable `[`intention_boost_weight`](#) |  Boost weight for intention matching. |
| double `variable `[`emotional_boost_weight`](#) |  Boost weight for emotional matching. |
| double `variable `[`min_similarity_score`](#) |  Minimum similarity score threshold. |
| struct `variable `[`boost_config`](#) |  |

## Members

### `default_top_k`

**Type**: int

 Default number of top results to return.

---

### `text_weight`

**Type**: double

 Weight for textual similarity in hybrid search.

---

### `semantic_weight`

**Type**: double

 Weight for semantic similarity in hybrid search.

---

### `intention_boost_weight`

**Type**: double

 Boost weight for intention matching.

---

### `emotional_boost_weight`

**Type**: double

 Boost weight for emotional matching.

---

### `min_similarity_score`

**Type**: double

 Minimum similarity score threshold.

---

### `boost_config`

**Type**: struct

---

