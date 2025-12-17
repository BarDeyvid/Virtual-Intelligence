# class `basic_json`

 a class to store JSON values

## Detailed Description

 namespace for Niels Lohmann version 1.0.0 version 1.0.0

## Summary

| Members | Descriptions |
|---------|--------------|
| `typedef `[`basic_json_t`](#) |  |
| ::nlohmann::detail::json_base_class< CustomBaseClass > `typedef `[`json_base_class_t`](#) |  |
| ::nlohmann::detail::primitive_iterator_t `typedef `[`primitive_iterator_t`](#) |  |
| ::nlohmann::detail::internal_iterator< BasicJsonType > `typedef `[`internal_iterator`](#) |  |
| ::nlohmann::detail::iter_impl< BasicJsonType > `typedef `[`iter_impl`](#) |  |
| ::nlohmann::detail::iteration_proxy< Iterator > `typedef `[`iteration_proxy`](#) |  |
| ::nlohmann::detail::json_reverse_iterator< Base > `typedef `[`json_reverse_iterator`](#) |  |
| ::nlohmann::detail::output_adapter_t< CharType > `typedef `[`output_adapter_t`](#) |  |
| ::nlohmann::detail::binary_reader< `typedef `[`binary_reader`](#) |  |
| ::nlohmann::detail::binary_writer< `typedef `[`binary_writer`](#) |  |
| `typedef `[`value_t`](#) |  |
| ::nlohmann::json_pointer< StringType > `typedef `[`json_pointer`](#) |  |
| JSONSerializer< T, SFINAE > `typedef `[`json_serializer`](#) |  |
| `typedef `[`error_handler_t`](#) |  |
| `typedef `[`cbor_tag_handler_t`](#) |  |
| `typedef `[`bjdata_version_t`](#) |  |
| std::initializer_list< `typedef `[`initializer_list_t`](#) |  |
| `typedef `[`input_format_t`](#) |  |
| `typedef `[`json_sax_t`](#) |  |
| { using `typedef `[`parse_error`](#) |  |
| `typedef `[`invalid_iterator`](#) |  |
| `typedef `[`type_error`](#) |  |
| `typedef `[`out_of_range`](#) |  |
| `typedef `[`other_error`](#) |  |
| { * the `typedef `[`reference`](#) |  |
| const `typedef `[`const_reference`](#) |  |
| std::ptrdiff_t `typedef `[`difference_type`](#) |  |
| std::size_t `typedef `[`size_type`](#) |  |
| AllocatorType< `typedef `[`allocator_type`](#) |  |
| typename std::allocator_traits< `typedef `[`pointer`](#) |  |
| typename std::allocator_traits< `typedef `[`const_pointer`](#) |  |
| `typedef `[`iterator`](#) |  |
| `typedef `[`const_iterator`](#) |  |
| `typedef `[`reverse_iterator`](#) |  |
| `typedef `[`const_reverse_iterator`](#) |  |
| struct `friend `[`detail::external_constructor`](#) |  |
| class `friend `[`::nlohmann::json_pointer`](#) |  |
| class `friend `[`::nlohmann::detail::parser`](#) |  |
| class `friend `[`::nlohmann::detail::iter_impl`](#) |  |
| class `friend `[`::nlohmann::detail::binary_writer`](#) |  |
| class `friend `[`::nlohmann::detail::binary_reader`](#) |  |
| class `friend `[`::nlohmann::detail::json_sax_dom_parser`](#) |  |
| class `friend `[`::nlohmann::detail::json_sax_dom_callback_parser`](#) |  |
| class `friend `[`::nlohmann::detail::exception`](#) |  |
| `variable `[`__pad0__`](#) |  |
| `variable `[`__pad1__`](#) |  |
| ** `variable `[`JSON_PRIVATE_UNLESS_TESTED`](#) |  |
| *boolean boolean_t `variable `[`boolean`](#) |  |
| *JSON `variable `[`Pointer`](#) |  |
| *SAX interface `variable `[`type`](#) |  |
| **brief returns the allocator associated with the container *sa `variable `[`https`](#) |  |
| *brief returns version information on the library *sa `variable `[`https`](#) |  |
| `variable `[`result`](#) |  |
| return `variable `[`result`](#) |  |
| ***name JSON value data types *The data types to store `variable `[`https`](#) |  |
| ***name JSON value data types *The data types to store `variable `[`basic_json`](#) |  |
| ***name JSON value data types *The data types to store `variable `[`default_object_comparator_t`](#) |  |
| ***name JSON value data types *The data types to store `variable `[`AllocatorType< std::pair< const StringType, basic_json > >`](#) |  |
| *brief `variable `[`https`](#) |  |
| *brief `variable `[`AllocatorType< basic_json >`](#) |  |
| *brief `variable `[`https`](#) |  |
| *brief `variable `[`https`](#) |  |
| *brief `variable `[`https`](#) |  |
| *brief object key comparator `variable `[`https`](#) |  |
| ***brief `variable `[`https`](#) |  |
| *brief per element `variable `[`https`](#) |  |
| ***name constructors and destructors *Constructors of class ref `variable `[`assignment`](#) |  |
| ***name constructors and destructors *Constructors of class ref `variable `[`m_data`](#) |  |
| ***name constructors and destructors *Constructors of class ref `variable `[`objects`](#) |  |
| ::nlohmann::detail::parser< `function `[`parser`](#) |  |
| * `function `[`array`](#) |  |
| * `function `[`string`](#) |  |
| * `function `[`binary`](#) |  |
| * `function `[`number`](#) |  |
| * `function `[`number`](#) |  |
| * `function `[`number`](#) |  |
| *default `function `[`constructor`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`numbers`](#) |  |
| * `function `[`numbers`](#) |  |
| * `function `[`numbers`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`json_value`](#) |  |
| * `function `[`arrays`](#) |  |
| * `function `[`arrays`](#) |  |
| void `function `[`destroy`](#) |  |
| void `function `[`assert_invariant`](#) |  checks the class invariants |
| void `function `[`set_parents`](#) |  |
| `function `[`set_parents`](#) |  |
| `function `[`set_parent`](#) |  |
| *helper for `function `[`create`](#) |  |

## Members

### `basic_json_t`

---

### `json_base_class_t`

**Type**: ::nlohmann::detail::json_base_class< CustomBaseClass >

---

### `primitive_iterator_t`

**Type**: ::nlohmann::detail::primitive_iterator_t

---

### `internal_iterator`

**Type**: ::nlohmann::detail::internal_iterator< BasicJsonType >

---

### `iter_impl`

**Type**: ::nlohmann::detail::iter_impl< BasicJsonType >

---

### `iteration_proxy`

**Type**: ::nlohmann::detail::iteration_proxy< Iterator >

---

### `json_reverse_iterator`

**Type**: ::nlohmann::detail::json_reverse_iterator< Base >

---

### `output_adapter_t`

**Type**: ::nlohmann::detail::output_adapter_t< CharType >

---

### `binary_reader`

**Type**: ::nlohmann::detail::binary_reader<

---

### `binary_writer`

**Type**: ::nlohmann::detail::binary_writer<

---

### `value_t`

---

### `json_pointer`

**Type**: ::nlohmann::json_pointer< StringType >

---

### `json_serializer`

**Type**: JSONSerializer< T, SFINAE >

---

### `error_handler_t`

---

### `cbor_tag_handler_t`

---

### `bjdata_version_t`

---

### `initializer_list_t`

**Type**: std::initializer_list<

---

### `input_format_t`

---

### `json_sax_t`

---

### `parse_error`

**Type**: { using

---

### `invalid_iterator`

---

### `type_error`

---

### `out_of_range`

---

### `other_error`

---

### `reference`

**Type**: { * the

---

### `const_reference`

**Type**: const

---

### `difference_type`

**Type**: std::ptrdiff_t

---

### `size_type`

**Type**: std::size_t

---

### `allocator_type`

**Type**: AllocatorType<

---

### `pointer`

**Type**: typename std::allocator_traits<

---

### `const_pointer`

**Type**: typename std::allocator_traits<

---

### `iterator`

---

### `const_iterator`

---

### `reverse_iterator`

---

### `const_reverse_iterator`

---

### `detail::external_constructor`

**Type**: struct

---

### `::nlohmann::json_pointer`

**Type**: class

---

### `::nlohmann::detail::parser`

**Type**: class

---

### `::nlohmann::detail::iter_impl`

**Type**: class

---

### `::nlohmann::detail::binary_writer`

**Type**: class

---

### `::nlohmann::detail::binary_reader`

**Type**: class

---

### `::nlohmann::detail::json_sax_dom_parser`

**Type**: class

---

### `::nlohmann::detail::json_sax_dom_callback_parser`

**Type**: class

---

### `::nlohmann::detail::exception`

**Type**: class

---

### `__pad0__`

---

### `__pad1__`

---

### `JSON_PRIVATE_UNLESS_TESTED`

**Type**: **

---

### `boolean`

**Type**: *boolean boolean_t

---

### `Pointer`

**Type**: *JSON

---

### `type`

**Type**: *SAX interface

---

### `https`

**Type**: **brief returns the allocator associated with the container *sa

---

### `https`

**Type**: *brief returns version information on the library *sa

---

### `result`

---

### `result`

**Type**: return

---

### `https`

**Type**: ***name JSON value data types *The data types to store

---

### `basic_json`

**Type**: ***name JSON value data types *The data types to store

---

### `default_object_comparator_t`

**Type**: ***name JSON value data types *The data types to store

---

### `AllocatorType< std::pair< const StringType, basic_json > >`

**Type**: ***name JSON value data types *The data types to store

---

### `https`

**Type**: *brief

---

### `AllocatorType< basic_json >`

**Type**: *brief

---

### `https`

**Type**: *brief

---

### `https`

**Type**: *brief

---

### `https`

**Type**: *brief

---

### `https`

**Type**: *brief object key comparator

---

### `https`

**Type**: ***brief

---

### `https`

**Type**: *brief per element

---

### `assignment`

**Type**: ***name constructors and destructors *Constructors of class ref

---

### `m_data`

**Type**: ***name constructors and destructors *Constructors of class ref

---

### `objects`

**Type**: ***name constructors and destructors *Constructors of class ref

---

### `parser`

**Type**: ::nlohmann::detail::parser<

---

### `array`

**Type**: *

---

### `string`

**Type**: *

---

### `binary`

**Type**: *

---

### `number`

**Type**: *

---

### `number`

**Type**: *

---

### `number`

**Type**: *

---

### `constructor`

**Type**: *default

---

### `json_value`

**Type**: *

---

### `numbers`

**Type**: *

---

### `numbers`

**Type**: *

---

### `numbers`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `json_value`

**Type**: *

---

### `arrays`

**Type**: *

---

### `arrays`

**Type**: *

---

### `destroy`

**Type**: void

---

### `assert_invariant`

**Type**: void

 checks the class invariants

---

### `set_parents`

**Type**: void

---

### `set_parents`

---

### `set_parent`

---

### `create`

**Type**: *helper for

---

