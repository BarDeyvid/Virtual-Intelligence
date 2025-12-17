# class `alyssa_memory::EmotionLexiconLoader`

 Handles loading and saving of emotion lexicons from/to JSON files.

## Detailed Description

 This class provides static methods to load emotion lexicons from JSON files, save lexicons to files, and access the default Portuguese lexicon. Lexicons are stored as a map where emotions are keys and word lists are values.

## Summary

| Members | Descriptions |
|---------|--------------|
| const std::string `variable `[`DEFAULT_LEXICON_PATH`](#) |  Default path to the Portuguese lexicon JSON file. |
| std::unordered_map< std::string, std::vector< std::string > > `function `[`loadDefaultPortugueseLexicon`](#) |  Loads the default Portuguese lexicon from the default file path. |
| std::unordered_map< std::string, std::vector< std::string > > `function `[`loadLexiconFromFile`](#) |  Loads an emotion lexicon from a specified JSON file. |
| void `function `[`saveLexiconToFile`](#) |  Saves an emotion lexicon to a JSON file. |

## Members

### `DEFAULT_LEXICON_PATH`

**Type**: const std::string

 Default path to the Portuguese lexicon JSON file.

---

### `loadDefaultPortugueseLexicon`

**Type**: std::unordered_map< std::string, std::vector< std::string > >

 Loads the default Portuguese lexicon from the default file path.

---

### `loadLexiconFromFile`

**Type**: std::unordered_map< std::string, std::vector< std::string > >

 Loads an emotion lexicon from a specified JSON file.

---

### `saveLexiconToFile`

**Type**: void

 Saves an emotion lexicon to a JSON file.

---

