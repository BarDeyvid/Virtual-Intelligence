# namespace `detail`

 detail namespace with internal helper functions

## Detailed Description

 This namespace collects functions that should not be exposed, implementations of some version 2.1.0

## Summary

| Members | Descriptions |
|---------|--------------|
| std::uint8_t `enum `[`value_t`](#) |  the JSON type enumeration |
| `enum `[`input_format_t`](#) |  the supported input formats |
| `enum `[`cbor_tag_handler_t`](#) |  how to treat CBOR tags |
| std::uint8_t `enum `[`parse_event_t`](#) |  |
| `enum `[`bjdata_version_t`](#) |  |
| `enum `[`error_handler_t`](#) |  |
| typename `typedef `[`void_t`](#) |  |
| typename `typedef `[`is_detected`](#) |  |
| typename `typedef `[`detected_t`](#) |  |
| `typedef `[`detected_or`](#) |  |
| typename `typedef `[`detected_or_t`](#) |  |
| std::is_same< Expected, `typedef `[`is_detected_exact`](#) |  |
| std::is_convertible< `typedef `[`is_detected_convertible`](#) |  |
| typename std::remove_cv< typename std::remove_reference< T >::type >::type `typedef `[`uncvref_t`](#) |  |
| typename std::enable_if< B, T >::type `typedef `[`enable_if_t`](#) |  |
| `typedef `[`index_sequence`](#) |  |
| typename `typedef `[`make_integer_sequence`](#) |  |
| `typedef `[`make_index_sequence`](#) |  |
| `typedef `[`index_sequence_for`](#) |  |
| typename T::mapped_type `typedef `[`mapped_type_t`](#) |  |
| typename T::key_type `typedef `[`key_type_t`](#) |  |
| typename T::value_type `typedef `[`value_type_t`](#) |  |
| typename T::difference_type `typedef `[`difference_type_t`](#) |  |
| typename T::pointer `typedef `[`pointer_t`](#) |  |
| typename T::reference `typedef `[`reference_t`](#) |  |
| typename T::iterator_category `typedef `[`iterator_category_t`](#) |  |
| decltype(T::to_json(std::declval< Args >()...)) `typedef `[`to_json_function`](#) |  |
| decltype(T::from_json(std::declval< Args >()...)) `typedef `[`from_json_function`](#) |  |
| decltype(std::declval< T >().template `typedef `[`get_template_function`](#) |  |
| typename T::key_compare `typedef `[`detect_key_compare`](#) |  |
| typename `typedef `[`actual_object_comparator_t`](#) |  |
| `typedef `[`iterator_t`](#) |  |
| `typedef `[`range_value_t`](#) |  |
| `typedef `[`is_json_pointer`](#) |  |
| typename T::is_transparent `typedef `[`detect_is_transparent`](#) |  |
| typename std::conditional< `typedef `[`is_usable_as_key_type`](#) |  |
| typename std::conditional< `typedef `[`is_usable_as_basic_json_key_type`](#) |  |
| decltype(std::declval< ObjectType & >(). `typedef `[`detect_erase_with_key_type`](#) |  |
| typename std::conditional< `typedef `[`has_erase_with_key_type`](#) |  |
| `typedef `[`all_integral`](#) |  |
| `typedef `[`all_signed`](#) |  |
| `typedef `[`all_unsigned`](#) |  |
| std::integral_constant< bool, `typedef `[`same_sign`](#) |  |
| std::integral_constant< bool,(std::is_signed< OfType > `typedef `[`never_out_of_range`](#) |  |
| std::integral_constant< bool, Value > `typedef `[`bool_constant`](#) |  |
| `typedef `[`is_c_string_uncvref`](#) |  |
| decltype(std::declval< StringType & >().append(std::declval< Arg && >())) `typedef `[`string_can_append`](#) |  |
| `typedef `[`detect_string_can_append`](#) |  |
| decltype(std::declval< StringType & >()+=std::declval< Arg && >()) `typedef `[`string_can_append_op`](#) |  |
| `typedef `[`detect_string_can_append_op`](#) |  |
| decltype(std::declval< StringType & >().append(std::declval< const Arg & >(). `typedef `[`string_can_append_iter`](#) |  |
| `typedef `[`detect_string_can_append_iter`](#) |  |
| decltype(std::declval< StringType & >().append(std::declval< const Arg & >().data(), std::declval< const Arg & >().size())) `typedef `[`string_can_append_data`](#) |  |
| `typedef `[`detect_string_can_append_data`](#) |  |
| decltype( `typedef `[`string_input_adapter_type`](#) |  |
| decltype( `typedef `[`contiguous_bytes_input_adapter`](#) |  |
| decltype(std::declval< T & >().null()) `typedef `[`null_function_t`](#) |  |
| decltype(std::declval< T & >().boolean(std::declval< bool >())) `typedef `[`boolean_function_t`](#) |  |
| decltype(std::declval< T & >().number_integer(std::declval< Integer >())) `typedef `[`number_integer_function_t`](#) |  |
| decltype(std::declval< T & >().number_unsigned(std::declval< Unsigned >())) `typedef `[`number_unsigned_function_t`](#) |  |
| decltype(std::declval< T & >().number_float( std::declval< Float >(), std::declval< const String & >())) `typedef `[`number_float_function_t`](#) |  |
| decltype(std::declval< T & >().string(std::declval< String & >())) `typedef `[`string_function_t`](#) |  |
| decltype(std::declval< T & >().binary(std::declval< Binary & >())) `typedef `[`binary_function_t`](#) |  |
| decltype(std::declval< T & >().start_object(std::declval< std::size_t >())) `typedef `[`start_object_function_t`](#) |  |
| decltype(std::declval< T & >().key(std::declval< String & >())) `typedef `[`key_function_t`](#) |  |
| decltype(std::declval< T & >().end_object()) `typedef `[`end_object_function_t`](#) |  |
| decltype(std::declval< T & >().start_array(std::declval< std::size_t >())) `typedef `[`start_array_function_t`](#) |  |
| decltype(std::declval< T & >().end_array()) `typedef `[`end_array_function_t`](#) |  |
| decltype(std::declval< T & >(). `typedef `[`parse_error_function_t`](#) |  |
| std::function< bool(int, `typedef `[`parser_callback_t`](#) |  |
| typename std::conditional< std::is_same< T, void > `typedef `[`json_base_class`](#) |  |
| std::shared_ptr< `typedef `[`output_adapter_t`](#) |  |
| T `variable `[`static_const< T >::value`](#) |  |
| std::size_t `variable `[`binary_reader< BasicJsonType, InputAdapterType, SAX >::npos`](#) |  |
| bool `function `[`operator<`](#) |  comparison operator for JSON types |
| void `function `[`replace_substring`](#) |  replace all occurrences of a substring by another string |
| StringType `function `[`escape`](#) |  string escaping as described in RFC 6901 (Sect. 4) |
| void `function `[`unescape`](#) |  string unescaping as described in RFC 6901 (Sect. 4) |
| std::array< T, sizeof...(Args)> `function `[`make_array`](#) |  |
| T `function `[`conditional_static_cast`](#) |  |
| T `function `[`conditional_static_cast`](#) |  |
| bool `function `[`value_in_range_of`](#) |  |
| std::size_t `function `[`concat_length`](#) |  |
| std::size_t `function `[`concat_length`](#) |  |
| std::size_t `function `[`concat_length`](#) |  |
| std::size_t `function `[`concat_length`](#) |  |
| void `function `[`concat_into`](#) |  |
| void `function `[`concat_into`](#) |  |
| void `function `[`concat_into`](#) |  |
| void `function `[`concat_into`](#) |  |
| void `function `[`concat_into`](#) |  |
| OutStringType `function `[`concat`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`get_arithmetic_value`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| decltype(j.template `function `[`from_json`](#) |  |
| decltype(j.template `function `[`from_json`](#) |  |
| decltype(j.template `function `[`from_json`](#) |  |
| decltype(j.template `function `[`from_json`](#) |  |
| void `function `[`from_json_array_impl`](#) |  |
| decltype(j.template `function `[`from_json_array_impl`](#) |  |
| decltype(arr.reserve(std::declval< typename ConstructibleArrayType::size_type >()), j.template `function `[`from_json_array_impl`](#) |  |
| void `function `[`from_json_array_impl`](#) |  |
| decltype( `function `[`from_json`](#) |  |
| std::array< T, sizeof...(Idx)> `function `[`from_json_inplace_array_impl`](#) |  |
| decltype( `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| std::tuple< Args... > `function `[`from_json_tuple_impl_base`](#) |  |
| std::tuple `function `[`from_json_tuple_impl_base`](#) |  |
| std::pair< A1, A2 > `function `[`from_json_tuple_impl`](#) |  |
| void `function `[`from_json_tuple_impl`](#) |  |
| std::tuple< Args... > `function `[`from_json_tuple_impl`](#) |  |
| void `function `[`from_json_tuple_impl`](#) |  |
| decltype( `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`from_json`](#) |  |
| void `function `[`int_to_string`](#) |  |
| StringType `function `[`to_string`](#) |  |
| decltype(i.key()) `function `[`get`](#) |  |
| decltype(i.value()) `function `[`get`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json`](#) |  |
| void `function `[`to_json_tuple_impl`](#) |  |
| void `function `[`to_json_tuple_impl`](#) |  |
| void `function `[`to_json`](#) |  |
| std::size_t `function `[`combine`](#) |  |
| std::size_t `function `[`hash`](#) |  hash a JSON value |
| `function `[`input_adapter`](#) |  |
| `function `[`input_adapter`](#) |  |
| `function `[`input_adapter`](#) |  |
| `function `[`input_adapter`](#) |  |
| `function `[`input_adapter`](#) |  |
| `function `[`input_adapter`](#) |  |
| decltype(input_adapter(array, array+N)) `function `[`input_adapter`](#) |  |
| std::size_t `function `[`unknown_size`](#) |  |
| bool `function `[`little_endianness`](#) |  determine system byte order |
| `function `[`to_chars`](#) |  generates a decimal representation of the floating-point number value in [first, last). |

## Members

### `value_t`

**Type**: std::uint8_t

 the JSON type enumeration

---

### `input_format_t`

 the supported input formats

---

### `cbor_tag_handler_t`

 how to treat CBOR tags

---

### `parse_event_t`

**Type**: std::uint8_t

---

### `bjdata_version_t`

---

### `error_handler_t`

---

### `void_t`

**Type**: typename

---

### `is_detected`

**Type**: typename

---

### `detected_t`

**Type**: typename

---

### `detected_or`

---

### `detected_or_t`

**Type**: typename

---

### `is_detected_exact`

**Type**: std::is_same< Expected,

---

### `is_detected_convertible`

**Type**: std::is_convertible<

---

### `uncvref_t`

**Type**: typename std::remove_cv< typename std::remove_reference< T >::type >::type

---

### `enable_if_t`

**Type**: typename std::enable_if< B, T >::type

---

### `index_sequence`

---

### `make_integer_sequence`

**Type**: typename

---

### `make_index_sequence`

---

### `index_sequence_for`

---

### `mapped_type_t`

**Type**: typename T::mapped_type

---

### `key_type_t`

**Type**: typename T::key_type

---

### `value_type_t`

**Type**: typename T::value_type

---

### `difference_type_t`

**Type**: typename T::difference_type

---

### `pointer_t`

**Type**: typename T::pointer

---

### `reference_t`

**Type**: typename T::reference

---

### `iterator_category_t`

**Type**: typename T::iterator_category

---

### `to_json_function`

**Type**: decltype(T::to_json(std::declval< Args >()...))

---

### `from_json_function`

**Type**: decltype(T::from_json(std::declval< Args >()...))

---

### `get_template_function`

**Type**: decltype(std::declval< T >().template

---

### `detect_key_compare`

**Type**: typename T::key_compare

---

### `actual_object_comparator_t`

**Type**: typename

---

### `iterator_t`

---

### `range_value_t`

---

### `is_json_pointer`

---

### `detect_is_transparent`

**Type**: typename T::is_transparent

---

### `is_usable_as_key_type`

**Type**: typename std::conditional<

---

### `is_usable_as_basic_json_key_type`

**Type**: typename std::conditional<

---

### `detect_erase_with_key_type`

**Type**: decltype(std::declval< ObjectType & >().

---

### `has_erase_with_key_type`

**Type**: typename std::conditional<

---

### `all_integral`

---

### `all_signed`

---

### `all_unsigned`

---

### `same_sign`

**Type**: std::integral_constant< bool,

---

### `never_out_of_range`

**Type**: std::integral_constant< bool,(std::is_signed< OfType >

---

### `bool_constant`

**Type**: std::integral_constant< bool, Value >

---

### `is_c_string_uncvref`

---

### `string_can_append`

**Type**: decltype(std::declval< StringType & >().append(std::declval< Arg && >()))

---

### `detect_string_can_append`

---

### `string_can_append_op`

**Type**: decltype(std::declval< StringType & >()+=std::declval< Arg && >())

---

### `detect_string_can_append_op`

---

### `string_can_append_iter`

**Type**: decltype(std::declval< StringType & >().append(std::declval< const Arg & >().

---

### `detect_string_can_append_iter`

---

### `string_can_append_data`

**Type**: decltype(std::declval< StringType & >().append(std::declval< const Arg & >().data(), std::declval< const Arg & >().size()))

---

### `detect_string_can_append_data`

---

### `string_input_adapter_type`

**Type**: decltype(

---

### `contiguous_bytes_input_adapter`

**Type**: decltype(

---

### `null_function_t`

**Type**: decltype(std::declval< T & >().null())

---

### `boolean_function_t`

**Type**: decltype(std::declval< T & >().boolean(std::declval< bool >()))

---

### `number_integer_function_t`

**Type**: decltype(std::declval< T & >().number_integer(std::declval< Integer >()))

---

### `number_unsigned_function_t`

**Type**: decltype(std::declval< T & >().number_unsigned(std::declval< Unsigned >()))

---

### `number_float_function_t`

**Type**: decltype(std::declval< T & >().number_float( std::declval< Float >(), std::declval< const String & >()))

---

### `string_function_t`

**Type**: decltype(std::declval< T & >().string(std::declval< String & >()))

---

### `binary_function_t`

**Type**: decltype(std::declval< T & >().binary(std::declval< Binary & >()))

---

### `start_object_function_t`

**Type**: decltype(std::declval< T & >().start_object(std::declval< std::size_t >()))

---

### `key_function_t`

**Type**: decltype(std::declval< T & >().key(std::declval< String & >()))

---

### `end_object_function_t`

**Type**: decltype(std::declval< T & >().end_object())

---

### `start_array_function_t`

**Type**: decltype(std::declval< T & >().start_array(std::declval< std::size_t >()))

---

### `end_array_function_t`

**Type**: decltype(std::declval< T & >().end_array())

---

### `parse_error_function_t`

**Type**: decltype(std::declval< T & >().

---

### `parser_callback_t`

**Type**: std::function< bool(int,

---

### `json_base_class`

**Type**: typename std::conditional< std::is_same< T, void >

---

### `output_adapter_t`

**Type**: std::shared_ptr<

---

### `static_const< T >::value`

**Type**: T

---

### `binary_reader< BasicJsonType, InputAdapterType, SAX >::npos`

**Type**: std::size_t

---

### `operator<`

**Type**: bool

 comparison operator for JSON types

---

### `replace_substring`

**Type**: void

 replace all occurrences of a substring by another string

---

### `escape`

**Type**: StringType

 string escaping as described in RFC 6901 (Sect. 4)

---

### `unescape`

**Type**: void

 string unescaping as described in RFC 6901 (Sect. 4)

---

### `make_array`

**Type**: std::array< T, sizeof...(Args)>

---

### `conditional_static_cast`

**Type**: T

---

### `conditional_static_cast`

**Type**: T

---

### `value_in_range_of`

**Type**: bool

---

### `concat_length`

**Type**: std::size_t

---

### `concat_length`

**Type**: std::size_t

---

### `concat_length`

**Type**: std::size_t

---

### `concat_length`

**Type**: std::size_t

---

### `concat_into`

**Type**: void

---

### `concat_into`

**Type**: void

---

### `concat_into`

**Type**: void

---

### `concat_into`

**Type**: void

---

### `concat_into`

**Type**: void

---

### `concat`

**Type**: OutStringType

---

### `from_json`

**Type**: void

---

### `get_arithmetic_value`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: decltype(j.template

---

### `from_json`

**Type**: decltype(j.template

---

### `from_json`

**Type**: decltype(j.template

---

### `from_json`

**Type**: decltype(j.template

---

### `from_json_array_impl`

**Type**: void

---

### `from_json_array_impl`

**Type**: decltype(j.template

---

### `from_json_array_impl`

**Type**: decltype(arr.reserve(std::declval< typename ConstructibleArrayType::size_type >()), j.template

---

### `from_json_array_impl`

**Type**: void

---

### `from_json`

**Type**: decltype(

---

### `from_json_inplace_array_impl`

**Type**: std::array< T, sizeof...(Idx)>

---

### `from_json`

**Type**: decltype(

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `from_json_tuple_impl_base`

**Type**: std::tuple< Args... >

---

### `from_json_tuple_impl_base`

**Type**: std::tuple

---

### `from_json_tuple_impl`

**Type**: std::pair< A1, A2 >

---

### `from_json_tuple_impl`

**Type**: void

---

### `from_json_tuple_impl`

**Type**: std::tuple< Args... >

---

### `from_json_tuple_impl`

**Type**: void

---

### `from_json`

**Type**: decltype(

---

### `from_json`

**Type**: void

---

### `from_json`

**Type**: void

---

### `int_to_string`

**Type**: void

---

### `to_string`

**Type**: StringType

---

### `get`

**Type**: decltype(i.key())

---

### `get`

**Type**: decltype(i.value())

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json`

**Type**: void

---

### `to_json_tuple_impl`

**Type**: void

---

### `to_json_tuple_impl`

**Type**: void

---

### `to_json`

**Type**: void

---

### `combine`

**Type**: std::size_t

---

### `hash`

**Type**: std::size_t

 hash a JSON value

---

### `input_adapter`

---

### `input_adapter`

---

### `input_adapter`

---

### `input_adapter`

---

### `input_adapter`

---

### `input_adapter`

---

### `input_adapter`

**Type**: decltype(input_adapter(array, array+N))

---

### `unknown_size`

**Type**: std::size_t

---

### `little_endianness`

**Type**: bool

 determine system byte order

---

### `to_chars`

 generates a decimal representation of the floating-point number value in [first, last).

---

