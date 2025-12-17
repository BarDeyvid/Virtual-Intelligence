# class `detail::wide_string_input_adapter`

## Summary

| Members | Descriptions |
|---------|--------------|
| char `typedef `[`char_type`](#) |  |
| BaseInputAdapter `variable `[`base_adapter`](#) |  |
| std::array< std::char_traits< char >::int_type, 4 > `variable `[`utf8_bytes`](#) |  a buffer for UTF-8 bytes |
| std::size_t `variable `[`utf8_bytes_index`](#) |  index to the utf8_codes array for the next valid byte |
| std::size_t `variable `[`utf8_bytes_filled`](#) |  number of valid bytes in the utf8_codes array |
| `function `[`wide_string_input_adapter`](#) |  |
| std::char_traits< char >::int_type `function `[`get_character`](#) |  |
| std::size_t `function `[`get_elements`](#) |  |
| void `function `[`fill_buffer`](#) |  |

## Members

### `char_type`

**Type**: char

---

### `base_adapter`

**Type**: BaseInputAdapter

---

### `utf8_bytes`

**Type**: std::array< std::char_traits< char >::int_type, 4 >

 a buffer for UTF-8 bytes

---

### `utf8_bytes_index`

**Type**: std::size_t

 index to the utf8_codes array for the next valid byte

---

### `utf8_bytes_filled`

**Type**: std::size_t

 number of valid bytes in the utf8_codes array

---

### `wide_string_input_adapter`

---

### `get_character`

**Type**: std::char_traits< char >::int_type

---

### `get_elements`

**Type**: std::size_t

---

### `fill_buffer`

**Type**: void

---

