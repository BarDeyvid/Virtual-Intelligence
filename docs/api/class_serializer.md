# class `detail::serializer`

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::binary_t::value_type `typedef `[`binary_char_t`](#) |  |
| std::uint8_t `variable `[`UTF8_ACCEPT`](#) |  |
| std::uint8_t `variable `[`UTF8_REJECT`](#) |  |
| `variable `[`__pad0__`](#) |  |
| `variable `[`ensure_ascii`](#) |  |
| std::uint8_t `variable `[`state`](#) |  |
| std::size_t `variable `[`bytes`](#) |  |
| std::size_t `variable `[`bytes_after_last_accept`](#) |  |
| std::size_t `variable `[`undumped_chars`](#) |  |
| `variable `[`else`](#) |  |
| `variable `[`enable_if_t< std::is_signed< NumberType >::value, int >`](#) |  |
| `variable `[`enable_if_t< std::is_unsigned< NumberType >::value, int >`](#) |  |
| * `variable `[`loc`](#) |  |
| *the locale s thousand separator character const char `variable `[`thousands_sep`](#) |  |
| *the locale s decimal point character const char `variable `[`decimal_point`](#) |  |
| *string buffer std::array< char, 512 > `variable `[`string_buffer`](#) |  |
| *the indentation character const char `variable `[`indent_char`](#) |  |
| *the indentation string `variable `[`indent_string`](#) |  |
| *error_handler how to react on decoding errors const `variable `[`error_handler`](#) |  |
| `function `[`serializer`](#) |  |
| `function `[`serializer`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`serializer`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~serializer`](#) |  |
| void `function `[`dump`](#) |  internal implementation of the serialization function |
| `function `[`for`](#) |  |
| `function `[`if`](#) |  |

## Members

### `string_t`

**Type**: typename BasicJsonType::string_t

---

### `number_float_t`

**Type**: typename BasicJsonType::number_float_t

---

### `number_integer_t`

**Type**: typename BasicJsonType::number_integer_t

---

### `number_unsigned_t`

**Type**: typename BasicJsonType::number_unsigned_t

---

### `binary_char_t`

**Type**: typename BasicJsonType::binary_t::value_type

---

### `UTF8_ACCEPT`

**Type**: std::uint8_t

---

### `UTF8_REJECT`

**Type**: std::uint8_t

---

### `__pad0__`

---

### `ensure_ascii`

---

### `state`

**Type**: std::uint8_t

---

### `bytes`

**Type**: std::size_t

---

### `bytes_after_last_accept`

**Type**: std::size_t

---

### `undumped_chars`

**Type**: std::size_t

---

### `else`

---

### `enable_if_t< std::is_signed< NumberType >::value, int >`

---

### `enable_if_t< std::is_unsigned< NumberType >::value, int >`

---

### `loc`

**Type**: *

---

### `thousands_sep`

**Type**: *the locale s thousand separator character const char

---

### `decimal_point`

**Type**: *the locale s decimal point character const char

---

### `string_buffer`

**Type**: *string buffer std::array< char, 512 >

---

### `indent_char`

**Type**: *the indentation character const char

---

### `indent_string`

**Type**: *the indentation string

---

### `error_handler`

**Type**: *error_handler how to react on decoding errors const

---

### `serializer`

---

### `serializer`

---

### `operator=`

---

### `serializer`

---

### `operator=`

---

### `~serializer`

---

### `dump`

**Type**: void

 internal implementation of the serialization function

---

### `for`

---

### `if`

---

