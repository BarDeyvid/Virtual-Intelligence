# struct `EmotionalAnalysis`

 Structure for emotional analysis results of text.

## Detailed Description

 Contains vector representation, dominant emotion, confidence score, and individual emotion scores.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::vector< float > `variable `[`emotional_vector`](#) |  Vector representation of emotional state. |
| std::string `variable `[`dominant_emotion`](#) |  Dominant emotion detected. |
| double `variable `[`confidence`](#) |  Confidence score of the analysis. |
| std::unordered_map< std::string, double > `variable `[`emotion_scores`](#) |  Individual emotion scores. |

## Members

### `emotional_vector`

**Type**: std::vector< float >

 Vector representation of emotional state.

---

### `dominant_emotion`

**Type**: std::string

 Dominant emotion detected.

---

### `confidence`

**Type**: double

 Confidence score of the analysis.

---

### `emotion_scores`

**Type**: std::unordered_map< std::string, double >

 Individual emotion scores.

---

