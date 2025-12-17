# class `EmotionalAnalyzer`

 Emotional analyzer for Portuguese text based on lexicon.

## Detailed Description

 Performs emotional analysis on text using lexicon matching, pattern analysis, and punctuation analysis to detect and quantify emotions.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::unordered_map< std::string, std::vector< std::string > > `variable `[`emotion_lexicons`](#) |  Emotion word lexicons. |
| std::unordered_map< std::string, double > `variable `[`emotion_weights`](#) |  Emotion category weights. |
| std::vector< std::string > `variable `[`emotion_categories`](#) |  Available emotion categories. |
| std::vector< std::unique_ptr< `variable `[`score_calculators_`](#) |  Score calculation strategies. |
| `variable `[`config_`](#) |  Configuration settings. |
| void `function `[`setupScoreCalculators`](#) |  Set up score calculators based on configuration. |
| void `function `[`initializeEmotionLexicons`](#) |  Initialize emotion lexicons from loader. |
| void `function `[`analyzeWithLexicon`](#) |  Analyze text using lexicon matching. |
| void `function `[`analyzePatterns`](#) |  Analyze patterns like intensifiers and negations. |
| void `function `[`analyzePunctuation`](#) |  Analyze punctuation for emotional cues. |
| std::vector< float > `function `[`normalizeScores`](#) |  Normalize emotion scores to vector format. |
| std::string `function `[`findDominantEmotion`](#) |  Find dominant emotion from scores. |
| double `function `[`calculateConfidence`](#) |  Calculate confidence score for analysis. |
| `function `[`EmotionalAnalyzer`](#) |  Constructor for |
| `function `[`analyzeText`](#) |  Analyze emotional content of text. |
| `function `[`analyzeConversation`](#) |  Analyze emotional content of conversation. |

## Members

### `emotion_lexicons`

**Type**: std::unordered_map< std::string, std::vector< std::string > >

 Emotion word lexicons.

---

### `emotion_weights`

**Type**: std::unordered_map< std::string, double >

 Emotion category weights.

---

### `emotion_categories`

**Type**: std::vector< std::string >

 Available emotion categories.

---

### `score_calculators_`

**Type**: std::vector< std::unique_ptr<

 Score calculation strategies.

---

### `config_`

 Configuration settings.

---

### `setupScoreCalculators`

**Type**: void

 Set up score calculators based on configuration.

---

### `initializeEmotionLexicons`

**Type**: void

 Initialize emotion lexicons from loader.

---

### `analyzeWithLexicon`

**Type**: void

 Analyze text using lexicon matching.

---

### `analyzePatterns`

**Type**: void

 Analyze patterns like intensifiers and negations.

---

### `analyzePunctuation`

**Type**: void

 Analyze punctuation for emotional cues.

---

### `normalizeScores`

**Type**: std::vector< float >

 Normalize emotion scores to vector format.

---

### `findDominantEmotion`

**Type**: std::string

 Find dominant emotion from scores.

---

### `calculateConfidence`

**Type**: double

 Calculate confidence score for analysis.

---

### `EmotionalAnalyzer`

 Constructor for

---

### `analyzeText`

 Analyze emotional content of text.

---

### `analyzeConversation`

 Analyze emotional content of conversation.

---

