# struct `EmotionalAnalyzerConfig`

 Configuration structure for emotional analyzer settings.

## Detailed Description

 This struct contains configuration parameters for the emotional analysis system, including emotion weights, scoring parameters, and conversation weighting.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::vector< `variable `[`default_weights`](#) |  |
| std::vector< std::string > `variable `[`emotion_categories`](#) |  |
| double `variable `[`lexicon_word_weight`](#) |  Weight for lexicon-based word matching. |
| double `variable `[`intesifier_boost`](#) |  Boost value for intensifier words. |
| double `variable `[`negation_penalty`](#) |  Penalty for negation words. |
| double `variable `[`exclamation_boost`](#) |  Boost for exclamation marks. |
| double `variable `[`question_boost`](#) |  Boost for question marks. |
| double `variable `[`period_frustation_boost`](#) |  Boost for multiple periods (frustration). |
| double `variable `[`min_confidence_threshold`](#) |  Minimum confidence threshold for emotion detection. |
| double `variable `[`user_input_weight`](#) |  Weight for user input in conversation analysis. |
| double `variable `[`ai_response_weight`](#) |  Weight for AI response in conversation analysis. |

## Members

### `default_weights`

**Type**: std::vector<

---

### `emotion_categories`

**Type**: std::vector< std::string >

---

### `lexicon_word_weight`

**Type**: double

 Weight for lexicon-based word matching.

---

### `intesifier_boost`

**Type**: double

 Boost value for intensifier words.

---

### `negation_penalty`

**Type**: double

 Penalty for negation words.

---

### `exclamation_boost`

**Type**: double

 Boost for exclamation marks.

---

### `question_boost`

**Type**: double

 Boost for question marks.

---

### `period_frustation_boost`

**Type**: double

 Boost for multiple periods (frustration).

---

### `min_confidence_threshold`

**Type**: double

 Minimum confidence threshold for emotion detection.

---

### `user_input_weight`

**Type**: double

 Weight for user input in conversation analysis.

---

### `ai_response_weight`

**Type**: double

 Weight for AI response in conversation analysis.

---

