# class `TextNormalizer`

 Text normalization utilities.

## Detailed Description

 Provides functions for normalizing text including case conversion, accent removal, and punctuation normalization.

## Summary

| Members | Descriptions |
|---------|--------------|
| const std::unordered_map< char, char > `variable `[`accent_map`](#) |  Map for accent conversion. |
| std::string `function `[`toLowerCase`](#) |  Convert text to lowercase. |
| std::string `function `[`removeExtraSpaces`](#) |  Remove extra whitespace from text. |
| std::string `function `[`normalizePunctuation`](#) |  Normalize punctuation spacing. |
| std::string `function `[`normalizePortugueseAccents`](#) |  Remove Portuguese accents from text. |
| std::vector< std::string > `function `[`extractWords`](#) |  Extract words from text with normalization. |

## Members

### `accent_map`

**Type**: const std::unordered_map< char, char >

 Map for accent conversion.

---

### `toLowerCase`

**Type**: std::string

 Convert text to lowercase.

---

### `removeExtraSpaces`

**Type**: std::string

 Remove extra whitespace from text.

---

### `normalizePunctuation`

**Type**: std::string

 Normalize punctuation spacing.

---

### `normalizePortugueseAccents`

**Type**: std::string

 Remove Portuguese accents from text.

---

### `extractWords`

**Type**: std::vector< std::string >

 Extract words from text with normalization.

---

