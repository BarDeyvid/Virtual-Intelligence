# class `LexiconScoreCalculator`

 Lexicon-based emotion score calculator.

## Detailed Description

 Calculates emotion scores based on word matching against emotion lexicons.

## Summary

| Members | Descriptions |
|---------|--------------|
| const std::unordered_map< std::string, std::vector< std::string > > & `variable `[`lexicons_`](#) |  Emotion lexicons. |
| const std::unordered_map< std::string, double > & `variable `[`weights_`](#) |  Emotion weights. |
| `function `[`LexiconScoreCalculator`](#) |  Constructor for |
| void `function `[`calculate`](#) |  Calculate emotion scores using lexicon matching. |

## Members

### `lexicons_`

**Type**: const std::unordered_map< std::string, std::vector< std::string > > &

 Emotion lexicons.

---

### `weights_`

**Type**: const std::unordered_map< std::string, double > &

 Emotion weights.

---

### `LexiconScoreCalculator`

 Constructor for

---

### `calculate`

**Type**: void

 Calculate emotion scores using lexicon matching.

---

