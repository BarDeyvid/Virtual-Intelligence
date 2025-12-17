# class `detail::binary_reader`

 deserialization of CBOR, MessagePack, and UBJSON values

## Summary

| Members | Descriptions |
|---------|--------------|
| typename BasicJsonType::number_integer_t `typedef `[`number_integer_t`](#) |  |
| typename BasicJsonType::number_unsigned_t `typedef `[`number_unsigned_t`](#) |  |
| typename BasicJsonType::number_float_t `typedef `[`number_float_t`](#) |  |
| typename BasicJsonType::string_t `typedef `[`string_t`](#) |  |
| typename BasicJsonType::binary_t `typedef `[`binary_t`](#) |  |
| SAX `typedef `[`json_sax_t`](#) |  |
| typename InputAdapterType::char_type `typedef `[`char_type`](#) |  |
| typename `typedef `[`char_int_type`](#) |  |
| std::pair< `typedef `[`bjd_type`](#) |  |
| `variable `[`npos`](#) |  |
| *input adapter InputAdapterType `variable `[`ia`](#) |  |
| *the current character `variable `[`current`](#) |  |
| *the number of characters read std::size_t `variable `[`chars_read`](#) |  |
| *whether we can assume little endianness const bool `variable `[`is_little_endian`](#) |  |
| *input format const `variable `[`input_format`](#) |  |
| *the SAX `variable `[`sax`](#) |  |
| `variable `[`__pad0__`](#) |  |
| const decltype( `variable `[`bjd_types_map`](#) |  |
| `function `[`binary_reader`](#) |  create a binary reader |
| `function `[`binary_reader`](#) |  |
| `function `[`binary_reader`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~binary_reader`](#) |  |
| bool `function `[`sax_parse`](#) |  |
| bool `function `[`parse_bson_internal`](#) |  Reads in a BSON-object and passes it to the SAX-parser. |
| bool `function `[`get_bson_string`](#) |  Parses a C-style string from the BSON input. |
| bool `function `[`get_bson_binary`](#) |  Parses a byte array input of length |
| bool `function `[`parse_bson_element_internal`](#) |  Read a BSON document element of the given |
| bool `function `[`parse_bson_element_list`](#) |  Read a BSON element list (as specified in the BSON-spec). |
| bool `function `[`parse_bson_array`](#) |  Reads an array from the BSON input and passes it to the SAX-parser. |
| **bool `function `[`parse_cbor_internal`](#) |  |
| bool `function `[`get_cbor_string`](#) |  reads a CBOR string |
| bool `function `[`get_cbor_binary`](#) |  reads a CBOR byte array |
| bool `function `[`get_cbor_array`](#) |  |
| bool `function `[`get_cbor_object`](#) |  |
| **bool `function `[`parse_msgpack_internal`](#) |  |
| bool `function `[`get_msgpack_string`](#) |  reads a MessagePack string |
| bool `function `[`get_msgpack_binary`](#) |  reads a MessagePack byte array |
| bool `function `[`get_msgpack_array`](#) |  |
| bool `function `[`get_msgpack_object`](#) |  |
| **bool `function `[`parse_ubjson_internal`](#) |  |
| bool `function `[`get_ubjson_string`](#) |  reads a UBJSON string |
| bool `function `[`get_ubjson_ndarray_size`](#) |  |
| bool `function `[`get_ubjson_size_value`](#) |  |
| bool `function `[`get_ubjson_size_type`](#) |  determine the type and size for a container |
| bool `function `[`get_ubjson_value`](#) |  |
| bool `function `[`get_ubjson_array`](#) |  |
| bool `function `[`get_ubjson_object`](#) |  |
| bool `function `[`get_ubjson_high_precision_number`](#) |  |
| ** `function `[`get`](#) |  get next character from the input |
| bool `function `[`get_to`](#) |  get_to read into a primitive type |
| `function `[`get_ignore_noop`](#) |  |
| bool `function `[`get_number`](#) |  |
| bool `function `[`get_string`](#) |  create a string by reading characters from the input |
| bool `function `[`get_binary`](#) |  create a byte array by reading bytes from the input |
| bool `function `[`unexpect_eof`](#) |  |
| std::string `function `[`get_token_string`](#) |  |
| std::string `function `[`exception_message`](#) |  |
| void `function `[`byte_swap`](#) |  |

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

### `json_sax_t`

**Type**: SAX

---

### `char_type`

**Type**: typename InputAdapterType::char_type

---

### `char_int_type`

**Type**: typename

---

### `bjd_type`

**Type**: std::pair<

---

### `npos`

---

### `ia`

**Type**: *input adapter InputAdapterType

---

### `current`

**Type**: *the current character

---

### `chars_read`

**Type**: *the number of characters read std::size_t

---

### `is_little_endian`

**Type**: *whether we can assume little endianness const bool

---

### `input_format`

**Type**: *input format const

---

### `sax`

**Type**: *the SAX

---

### `__pad0__`

---

### `bjd_types_map`

**Type**: const decltype(

---

### `binary_reader`

 create a binary reader

---

### `binary_reader`

---

### `binary_reader`

---

### `operator=`

---

### `operator=`

---

### `~binary_reader`

---

### `sax_parse`

**Type**: bool

---

### `parse_bson_internal`

**Type**: bool

 Reads in a BSON-object and passes it to the SAX-parser.

---

### `get_bson_string`

**Type**: bool

 Parses a C-style string from the BSON input.

---

### `get_bson_binary`

**Type**: bool

 Parses a byte array input of length

---

### `parse_bson_element_internal`

**Type**: bool

 Read a BSON document element of the given

---

### `parse_bson_element_list`

**Type**: bool

 Read a BSON element list (as specified in the BSON-spec).

---

### `parse_bson_array`

**Type**: bool

 Reads an array from the BSON input and passes it to the SAX-parser.

---

### `parse_cbor_internal`

**Type**: **bool

---

### `get_cbor_string`

**Type**: bool

 reads a CBOR string

---

### `get_cbor_binary`

**Type**: bool

 reads a CBOR byte array

---

### `get_cbor_array`

**Type**: bool

---

### `get_cbor_object`

**Type**: bool

---

### `parse_msgpack_internal`

**Type**: **bool

---

### `get_msgpack_string`

**Type**: bool

 reads a MessagePack string

---

### `get_msgpack_binary`

**Type**: bool

 reads a MessagePack byte array

---

### `get_msgpack_array`

**Type**: bool

---

### `get_msgpack_object`

**Type**: bool

---

### `parse_ubjson_internal`

**Type**: **bool

---

### `get_ubjson_string`

**Type**: bool

 reads a UBJSON string

---

### `get_ubjson_ndarray_size`

**Type**: bool

---

### `get_ubjson_size_value`

**Type**: bool

---

### `get_ubjson_size_type`

**Type**: bool

 determine the type and size for a container

---

### `get_ubjson_value`

**Type**: bool

---

### `get_ubjson_array`

**Type**: bool

---

### `get_ubjson_object`

**Type**: bool

---

### `get_ubjson_high_precision_number`

**Type**: bool

---

### `get`

**Type**: **

 get next character from the input

---

### `get_to`

**Type**: bool

 get_to read into a primitive type

---

### `get_ignore_noop`

---

### `get_number`

**Type**: bool

---

### `get_string`

**Type**: bool

 create a string by reading characters from the input

---

### `get_binary`

**Type**: bool

 create a byte array by reading bytes from the input

---

### `unexpect_eof`

**Type**: bool

---

### `get_token_string`

**Type**: std::string

---

### `exception_message`

**Type**: std::string

---

### `byte_swap`

**Type**: void

---

