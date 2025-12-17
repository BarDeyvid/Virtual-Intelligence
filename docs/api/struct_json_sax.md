# struct `json_sax`

 SAX interface.

## Detailed Description

 This class describes the SAX interface used by nlohmann::json::sax_parse. Each function is called in different situations while the input is parsed. The boolean return value informs the parser whether to continue processing the input.

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename BasicJsonType::binary_t `typedef `[`binary_t`](#) |  |
| bool `function `[`null`](#) |  a null value was read |
| bool `function `[`boolean`](#) |  a boolean value was read |
| bool `function `[`number_integer`](#) |  an integer number was read |
| bool `function `[`number_unsigned`](#) |  an unsigned integer number was read |
| bool `function `[`number_float`](#) |  a floating-point number was read |
| bool `function `[`string`](#) |  a string value was read |
| bool `function `[`binary`](#) |  a binary value was read |
| bool `function `[`start_object`](#) |  the beginning of an object was read |
| bool `function `[`key`](#) |  an object key was read |
| bool `function `[`end_object`](#) |  the end of an object was read |
| bool `function `[`start_array`](#) |  the beginning of an array was read |
| bool `function `[`end_array`](#) |  the end of an array was read |
| bool `function `[`parse_error`](#) |  a parse error occurred |
| `function `[`json_sax`](#) |  |
| `function `[`json_sax`](#) |  |
| `function `[`json_sax`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~json_sax`](#) |  |

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

### `null`

**Type**: bool

 a null value was read

---

### `boolean`

**Type**: bool

 a boolean value was read

---

### `number_integer`

**Type**: bool

 an integer number was read

---

### `number_unsigned`

**Type**: bool

 an unsigned integer number was read

---

### `number_float`

**Type**: bool

 a floating-point number was read

---

### `string`

**Type**: bool

 a string value was read

---

### `binary`

**Type**: bool

 a binary value was read

---

### `start_object`

**Type**: bool

 the beginning of an object was read

---

### `key`

**Type**: bool

 an object key was read

---

### `end_object`

**Type**: bool

 the end of an object was read

---

### `start_array`

**Type**: bool

 the beginning of an array was read

---

### `end_array`

**Type**: bool

 the end of an array was read

---

### `parse_error`

**Type**: bool

 a parse error occurred

---

### `json_sax`

---

### `json_sax`

---

### `json_sax`

---

### `operator=`

---

### `operator=`

---

### `~json_sax`

---

