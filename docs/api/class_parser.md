# class `detail::parser`

 syntax analysis

## Detailed Description

 This class implements a recursive descent parser.

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| `typedef `[`lexer_t`](#) |  |
| typename `typedef `[`token_type`](#) |  |
| *callback function const `variable `[`callback`](#) |  |
| *the type of the last read `variable `[`last_token`](#) |  |
| *the `variable `[`m_lexer`](#) |  |
| *whether to throw exceptions in case of errors const bool `variable `[`allow_exceptions`](#) |  |
| * `function `[`parser`](#) |  |
| void `function `[`parse`](#) |  public parser interface |
| bool `function `[`accept`](#) |  public accept interface |
| bool `function `[`sax_parse`](#) |  |
| bool `function `[`sax_parse_internal`](#) |  |
| * `function `[`get_token`](#) |  |
| std::string `function `[`exception_message`](#) |  |
| *whether trailing commas in objects and arrays should be `function `[`ignored`](#) |  |

## Members

### `number_integer_t`

**Type**: typename BasicJsonType::number_integer_t

---

### `number_unsigned_t`

**Type**: typename BasicJsonType::number_unsigned_t

---

### `number_float_t`

**Type**: typename BasicJsonType::number_float_t

---

### `string_t`

**Type**: typename BasicJsonType::string_t

---

### `lexer_t`

---

### `token_type`

**Type**: typename

---

### `callback`

**Type**: *callback function const

---

### `last_token`

**Type**: *the type of the last read

---

### `m_lexer`

**Type**: *the

---

### `allow_exceptions`

**Type**: *whether to throw exceptions in case of errors const bool

---

### `parser`

**Type**: *

---

### `parse`

**Type**: void

 public parser interface

---

### `accept`

**Type**: bool

 public accept interface

---

### `sax_parse`

**Type**: bool

---

### `sax_parse_internal`

**Type**: bool

---

### `get_token`

**Type**: *

---

### `exception_message`

**Type**: std::string

---

### `ignored`

**Type**: *whether trailing commas in objects and arrays should be

---

