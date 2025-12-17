# struct `alyssa_fusion::ExpertContribution`

 Represents the contribution of an expert with its response and metadata.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::string `variable `[`expert_id`](#) |  Unique identifier for the expert. |
| double `variable `[`weight`](#) |  Weight assigned to the expert's response. |
| std::string `variable `[`response`](#) |  The expert's response. |
| std::vector< float > `variable `[`embedding`](#) |  Embedding of the expert's response. |
| std::string `variable `[`source`](#) |  Source or context of the response. |

## Members

### `expert_id`

**Type**: std::string

 Unique identifier for the expert.

---

### `weight`

**Type**: double

 Weight assigned to the expert's response.

---

### `response`

**Type**: std::string

 The expert's response.

---

### `embedding`

**Type**: std::vector< float >

 Embedding of the expert's response.

---

### `source`

**Type**: std::string

 Source or context of the response.

---

