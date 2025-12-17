# class `detail::input_stream_adapter`

## Detailed Description

 Input adapter for a (caching) istream. Ignores a UFT Byte Order Mark at beginning of input. Does not support changing the underlying std::streambuf in mid-input. Maintains underlying std::istream and std::streambuf to support subsequent use of standard std::istream operations to process any input characters following those used in parsing the JSON input. Clears the std::istream flags; any input errors (e.g., EOF) will be detected by the first subsequent call for input from the std::istream.

## Summary

| Members | Descriptions |
|---------|--------------|
| char `typedef `[`char_type`](#) |  |
| std::istream * `variable `[`is`](#) |  the associated input stream |
| std::streambuf * `variable `[`sb`](#) |  |
| `function `[`~input_stream_adapter`](#) |  |
| `function `[`input_stream_adapter`](#) |  |
| `function `[`input_stream_adapter`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`input_stream_adapter`](#) |  |
| std::char_traits< char >::int_type `function `[`get_character`](#) |  |
| std::size_t `function `[`get_elements`](#) |  |

## Members

### `char_type`

**Type**: char

---

### `is`

**Type**: std::istream *

 the associated input stream

---

### `sb`

**Type**: std::streambuf *

---

### `~input_stream_adapter`

---

### `input_stream_adapter`

---

### `input_stream_adapter`

---

### `operator=`

---

### `operator=`

---

### `input_stream_adapter`

---

### `get_character`

**Type**: std::char_traits< char >::int_type

---

### `get_elements`

**Type**: std::size_t

---

