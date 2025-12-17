# class `detail::json_sax_dom_parser`

 SAX implementation to create a JSON value from SAX events.

## Detailed Description

 This class implements the After successful parsing, the value that is passed by reference to the constructor contains the parsed value. the JSON type

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename BasicJsonType::binary_t `typedef `[`binary_t`](#) |  |
| `typedef `[`lexer_t`](#) |  |
| BasicJsonType & `variable `[`root`](#) |  the parsed JSON value |
| std::vector< BasicJsonType * > `variable `[`ref_stack`](#) |  stack to model hierarchy of values |
| BasicJsonType * `variable `[`object_element`](#) |  helper to hold the reference for the next object element |
| bool `variable `[`errored`](#) |  whether a syntax error occurred |
| const bool `variable `[`allow_exceptions`](#) |  whether to throw exceptions in case of errors |
| `variable `[`m_lexer_ref`](#) |  the lexer reference to obtain the current position |
| `function `[`json_sax_dom_parser`](#) |  |
| `function `[`json_sax_dom_parser`](#) |  |
| `function `[`json_sax_dom_parser`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~json_sax_dom_parser`](#) |  |
| bool `function `[`null`](#) |  |
| bool `function `[`boolean`](#) |  |
| bool `function `[`number_integer`](#) |  |
| bool `function `[`number_unsigned`](#) |  |
| bool `function `[`number_float`](#) |  |
| bool `function `[`string`](#) |  |
| bool `function `[`binary`](#) |  |
| bool `function `[`start_object`](#) |  |
| bool `function `[`key`](#) |  |
| bool `function `[`end_object`](#) |  |
| bool `function `[`start_array`](#) |  |
| bool `function `[`end_array`](#) |  |
| bool `function `[`parse_error`](#) |  |
| bool `function `[`is_errored`](#) |  |
| `function `[`handle_value`](#) |  |

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

### `binary_t`

**Type**: typename BasicJsonType::binary_t

---

### `lexer_t`

---

### `root`

**Type**: BasicJsonType &

 the parsed JSON value

---

### `ref_stack`

**Type**: std::vector< BasicJsonType * >

 stack to model hierarchy of values

---

### `object_element`

**Type**: BasicJsonType *

 helper to hold the reference for the next object element

---

### `errored`

**Type**: bool

 whether a syntax error occurred

---

### `allow_exceptions`

**Type**: const bool

 whether to throw exceptions in case of errors

---

### `m_lexer_ref`

 the lexer reference to obtain the current position

---

### `json_sax_dom_parser`

---

### `json_sax_dom_parser`

---

### `json_sax_dom_parser`

---

### `operator=`

---

### `operator=`

---

### `~json_sax_dom_parser`

---

### `null`

**Type**: bool

---

### `boolean`

**Type**: bool

---

### `number_integer`

**Type**: bool

---

### `number_unsigned`

**Type**: bool

---

### `number_float`

**Type**: bool

---

### `string`

**Type**: bool

---

### `binary`

**Type**: bool

---

### `start_object`

**Type**: bool

---

### `key`

**Type**: bool

---

### `end_object`

**Type**: bool

---

### `start_array`

**Type**: bool

---

### `end_array`

**Type**: bool

---

### `parse_error`

**Type**: bool

---

### `is_errored`

**Type**: bool

---

### `handle_value`

---

