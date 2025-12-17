# class `detail::json_sax_dom_callback_parser`

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename BasicJsonType::binary_t `typedef `[`binary_t`](#) |  |
| typename BasicJsonType::parser_callback_t `typedef `[`parser_callback_t`](#) |  |
| typename BasicJsonType::parse_event_t `typedef `[`parse_event_t`](#) |  |
| `typedef `[`lexer_t`](#) |  |
| BasicJsonType & `variable `[`root`](#) |  the parsed JSON value |
| std::vector< BasicJsonType * > `variable `[`ref_stack`](#) |  stack to model hierarchy of values |
| std::vector< bool > `variable `[`keep_stack`](#) |  stack to manage which values to keep |
| std::vector< bool > `variable `[`key_keep_stack`](#) |  stack to manage which object keys to keep |
| BasicJsonType * `variable `[`object_element`](#) |  helper to hold the reference for the next object element |
| bool `variable `[`errored`](#) |  whether a syntax error occurred |
| const `variable `[`callback`](#) |  callback function |
| const bool `variable `[`allow_exceptions`](#) |  whether to throw exceptions in case of errors |
| BasicJsonType `variable `[`discarded`](#) |  a discarded value for the callback |
| `variable `[`m_lexer_ref`](#) |  the lexer reference to obtain the current position |
| `function `[`json_sax_dom_callback_parser`](#) |  |
| `function `[`json_sax_dom_callback_parser`](#) |  |
| `function `[`json_sax_dom_callback_parser`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~json_sax_dom_callback_parser`](#) |  |
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
| std::pair< bool, BasicJsonType * > `function `[`handle_value`](#) |  |

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

### `parser_callback_t`

**Type**: typename BasicJsonType::parser_callback_t

---

### `parse_event_t`

**Type**: typename BasicJsonType::parse_event_t

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

### `keep_stack`

**Type**: std::vector< bool >

 stack to manage which values to keep

---

### `key_keep_stack`

**Type**: std::vector< bool >

 stack to manage which object keys to keep

---

### `object_element`

**Type**: BasicJsonType *

 helper to hold the reference for the next object element

---

### `errored`

**Type**: bool

 whether a syntax error occurred

---

### `callback`

**Type**: const

 callback function

---

### `allow_exceptions`

**Type**: const bool

 whether to throw exceptions in case of errors

---

### `discarded`

**Type**: BasicJsonType

 a discarded value for the callback

---

### `m_lexer_ref`

 the lexer reference to obtain the current position

---

### `json_sax_dom_callback_parser`

---

### `json_sax_dom_callback_parser`

---

### `json_sax_dom_callback_parser`

---

### `operator=`

---

### `operator=`

---

### `~json_sax_dom_callback_parser`

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

**Type**: std::pair< bool, BasicJsonType * >

---

