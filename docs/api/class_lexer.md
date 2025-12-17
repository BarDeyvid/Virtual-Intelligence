# class `detail::lexer`

 lexical analysis

## Detailed Description

 This class organizes the lexical analysis during JSON deserialization.

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename InputAdapterType::char_type `typedef `[`char_type`](#) |  |
| typename `typedef `[`char_int_type`](#) |  |
| typename `typedef `[`token_type`](#) |  |
| InputAdapterType `variable `[`ia`](#) |  input adapter |
| const bool `variable `[`ignore_comments`](#) |  whether comments should be ignored (true) or signaled as errors (false) |
| `variable `[`current`](#) |  the current character |
| bool `variable `[`next_unget`](#) |  whether the next |
| `variable `[`position`](#) |  the start position of the current token |
| std::vector< `variable `[`token_string`](#) |  raw input token string (for error messages) |
| `variable `[`token_buffer`](#) |  buffer for variable-length tokens (numbers, strings) |
| const char * `variable `[`error_message`](#) |  a description of occurred lexer errors |
| `variable `[`value_integer`](#) |  |
| `variable `[`value_unsigned`](#) |  |
| `variable `[`value_float`](#) |  |
| const `variable `[`decimal_point_char`](#) |  the decimal point |
| std::size_t `variable `[`decimal_point_position`](#) |  the position of the decimal point in the input |
| `function `[`lexer`](#) |  |
| `function `[`lexer`](#) |  |
| `function `[`lexer`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~lexer`](#) |  |
| `function `[`get_number_integer`](#) |  return integer value |
| `function `[`get_number_unsigned`](#) |  return unsigned integer value |
| `function `[`get_number_float`](#) |  return floating-point value |
| `function `[`get_string`](#) |  return current string value (implicitly resets the token; useful only once) |
| `function `[`get_position`](#) |  return position of last read token |
| std::string `function `[`get_token_string`](#) |  return the last read token (for errors only). |
| `function `[`get_error_message`](#) |  return syntax error message |
| bool `function `[`skip_bom`](#) |  skip the UTF-8 byte order mark |
| void `function `[`skip_whitespace`](#) |  |
| `function `[`scan`](#) |  |
| `function `[`get_decimal_point`](#) |  return the locale-dependent decimal point |
| void `function `[`strtof`](#) |  |
| void `function `[`strtof`](#) |  |
| void `function `[`strtof`](#) |  |
| int `function `[`get_codepoint`](#) |  get codepoint from 4 hex characters following |
| bool `function `[`next_byte_in_range`](#) |  check if the next byte(s) are inside a given range |
| `function `[`scan_string`](#) |  scan a string literal |
| bool `function `[`scan_comment`](#) |  scan a comment |
| `function `[`scan_number`](#) |  scan a number literal |
| `function `[`scan_literal`](#) |  |
| void `function `[`reset`](#) |  reset token_buffer; current character is beginning of token |
| `function `[`get`](#) |  |
| void `function `[`unget`](#) |  unget current character (read it again on next get) |
| void `function `[`add`](#) |  add a character to token_buffer |

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

### `char_type`

**Type**: typename InputAdapterType::char_type

---

### `char_int_type`

**Type**: typename

---

### `token_type`

**Type**: typename

---

### `ia`

**Type**: InputAdapterType

 input adapter

---

### `ignore_comments`

**Type**: const bool

 whether comments should be ignored (true) or signaled as errors (false)

---

### `current`

 the current character

---

### `next_unget`

**Type**: bool

 whether the next

---

### `position`

 the start position of the current token

---

### `token_string`

**Type**: std::vector<

 raw input token string (for error messages)

---

### `token_buffer`

 buffer for variable-length tokens (numbers, strings)

---

### `error_message`

**Type**: const char *

 a description of occurred lexer errors

---

### `value_integer`

---

### `value_unsigned`

---

### `value_float`

---

### `decimal_point_char`

**Type**: const

 the decimal point

---

### `decimal_point_position`

**Type**: std::size_t

 the position of the decimal point in the input

---

### `lexer`

---

### `lexer`

---

### `lexer`

---

### `operator=`

---

### `operator=`

---

### `~lexer`

---

### `get_number_integer`

 return integer value

---

### `get_number_unsigned`

 return unsigned integer value

---

### `get_number_float`

 return floating-point value

---

### `get_string`

 return current string value (implicitly resets the token; useful only once)

---

### `get_position`

 return position of last read token

---

### `get_token_string`

**Type**: std::string

 return the last read token (for errors only).

---

### `get_error_message`

 return syntax error message

---

### `skip_bom`

**Type**: bool

 skip the UTF-8 byte order mark

---

### `skip_whitespace`

**Type**: void

---

### `scan`

---

### `get_decimal_point`

 return the locale-dependent decimal point

---

### `strtof`

**Type**: void

---

### `strtof`

**Type**: void

---

### `strtof`

**Type**: void

---

### `get_codepoint`

**Type**: int

 get codepoint from 4 hex characters following

---

### `next_byte_in_range`

**Type**: bool

 check if the next byte(s) are inside a given range

---

### `scan_string`

 scan a string literal

---

### `scan_comment`

**Type**: bool

 scan a comment

---

### `scan_number`

 scan a number literal

---

### `scan_literal`

---

### `reset`

**Type**: void

 reset token_buffer; current character is beginning of token

---

### `get`

---

### `unget`

**Type**: void

 unget current character (read it again on next get)

---

### `add`

**Type**: void

 add a character to token_buffer

---

