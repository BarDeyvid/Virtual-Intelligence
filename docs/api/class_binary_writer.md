# class `detail::binary_writer`

 serialization to CBOR and MessagePack values

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename BasicJsonType::binary_t `typedef `[`binary_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| *whether we can assume little endianness const bool `variable `[`is_little_endian`](#) |  |
| *the output `variable `[`oa`](#) |  |
| `function `[`binary_writer`](#) |  create a binary writer |
| void `function `[`write_bson`](#) |  |
| void `function `[`write_cbor`](#) |  |
| void `function `[`write_msgpack`](#) |  |
| void `function `[`write_ubjson`](#) |  |
| **static std::size_t `function `[`calc_bson_entry_header_size`](#) |  |
| std::size_t `function `[`calc_bson_string_size`](#) |  |
| std::size_t `function `[`calc_bson_integer_size`](#) |  |
| std::size_t `function `[`calc_bson_unsigned_size`](#) |  |
| std::size_t `function `[`calc_bson_array_size`](#) |  |
| std::size_t `function `[`calc_bson_binary_size`](#) |  |
| std::size_t `function `[`calc_bson_element_size`](#) |  Calculates the size necessary to serialize the JSON value |
| std::size_t `function `[`calc_bson_object_size`](#) |  Calculates the size of the BSON serialization of the given JSON-object |
| **static constexpr CharType `function `[`get_cbor_float_prefix`](#) |  |
| CharType `function `[`get_cbor_float_prefix`](#) |  |
| **static constexpr CharType `function `[`get_msgpack_float_prefix`](#) |  |
| CharType `function `[`get_msgpack_float_prefix`](#) |  |
| CharType `function `[`get_ubjson_float_prefix`](#) |  |
| CharType `function `[`get_ubjson_float_prefix`](#) |  |
| void `function `[`write_bson_entry_header`](#) |  Writes the given |
| void `function `[`write_bson_boolean`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_double`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_string`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_null`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_integer`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_unsigned`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_object_entry`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_array`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_binary`](#) |  Writes a BSON element with key |
| void `function `[`write_bson_element`](#) |  Serializes the JSON value |
| void `function `[`write_bson_object`](#) |  |
| **void `function `[`write_number_with_ubjson_prefix`](#) |  |
| void `function `[`write_number_with_ubjson_prefix`](#) |  |
| void `function `[`write_number_with_ubjson_prefix`](#) |  |
| CharType `function `[`ubjson_prefix`](#) |  determine the type prefix of container values |
| bool `function `[`write_bjdata_ndarray`](#) |  |
| **void `function `[`write_number`](#) |  |
| void `function `[`write_compact_float`](#) |  |
| CharType `function `[`to_char_type`](#) |  |
| CharType `function `[`to_char_type`](#) |  |
| CharType `function `[`to_char_type`](#) |  |
| CharType `function `[`to_char_type`](#) |  |

## Members

### `string_t`

**Type**: typename BasicJsonType::string_t

---

### `binary_t`

**Type**: typename BasicJsonType::binary_t

---

### `number_float_t`

**Type**: typename BasicJsonType::number_float_t

---

### `is_little_endian`

**Type**: *whether we can assume little endianness const bool

---

### `oa`

**Type**: *the output

---

### `binary_writer`

 create a binary writer

---

### `write_bson`

**Type**: void

---

### `write_cbor`

**Type**: void

---

### `write_msgpack`

**Type**: void

---

### `write_ubjson`

**Type**: void

---

### `calc_bson_entry_header_size`

**Type**: **static std::size_t

---

### `calc_bson_string_size`

**Type**: std::size_t

---

### `calc_bson_integer_size`

**Type**: std::size_t

---

### `calc_bson_unsigned_size`

**Type**: std::size_t

---

### `calc_bson_array_size`

**Type**: std::size_t

---

### `calc_bson_binary_size`

**Type**: std::size_t

---

### `calc_bson_element_size`

**Type**: std::size_t

 Calculates the size necessary to serialize the JSON value

---

### `calc_bson_object_size`

**Type**: std::size_t

 Calculates the size of the BSON serialization of the given JSON-object

---

### `get_cbor_float_prefix`

**Type**: **static constexpr CharType

---

### `get_cbor_float_prefix`

**Type**: CharType

---

### `get_msgpack_float_prefix`

**Type**: **static constexpr CharType

---

### `get_msgpack_float_prefix`

**Type**: CharType

---

### `get_ubjson_float_prefix`

**Type**: CharType

---

### `get_ubjson_float_prefix`

**Type**: CharType

---

### `write_bson_entry_header`

**Type**: void

 Writes the given

---

### `write_bson_boolean`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_double`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_string`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_null`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_integer`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_unsigned`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_object_entry`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_array`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_binary`

**Type**: void

 Writes a BSON element with key

---

### `write_bson_element`

**Type**: void

 Serializes the JSON value

---

### `write_bson_object`

**Type**: void

---

### `write_number_with_ubjson_prefix`

**Type**: **void

---

### `write_number_with_ubjson_prefix`

**Type**: void

---

### `write_number_with_ubjson_prefix`

**Type**: void

---

### `ubjson_prefix`

**Type**: CharType

 determine the type prefix of container values

---

### `write_bjdata_ndarray`

**Type**: bool

---

### `write_number`

**Type**: **void

---

### `write_compact_float`

**Type**: void

---

### `to_char_type`

**Type**: CharType

---

### `to_char_type`

**Type**: CharType

---

### `to_char_type`

**Type**: CharType

---

### `to_char_type`

**Type**: CharType

---

