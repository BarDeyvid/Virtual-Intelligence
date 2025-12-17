# class `detail::iteration_proxy_value`

## Summary

| Members | Descriptions |
|---------|--------------|
| std::ptrdiff_t `typedef `[`difference_type`](#) |  |
| `typedef `[`value_type`](#) |  |
| `typedef `[`pointer`](#) |  |
| `typedef `[`reference`](#) |  |
| std::forward_iterator_tag `typedef `[`iterator_category`](#) |  |
| typename std::remove_cv< typename std::remove_reference< decltype(std::declval< IteratorType >(). `typedef `[`string_type`](#) |  |
| IteratorType `variable `[`anchor`](#) |  the iterator |
| std::size_t `variable `[`array_index`](#) |  an index for arrays (used to create key names) |
| std::size_t `variable `[`array_index_last`](#) |  last stringified array index |
| `variable `[`array_index_str`](#) |  a string representation of the array index |
| `variable `[`empty_str`](#) |  an empty string (to return a reference for primitive values) |
| `function `[`iteration_proxy_value`](#) |  |
| `function `[`iteration_proxy_value`](#) |  |
| `function `[`iteration_proxy_value`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`iteration_proxy_value`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`~iteration_proxy_value`](#) |  |
| const `function `[`operator*`](#) |  dereference operator (needed for range-based for) |
| `function `[`operator++`](#) |  increment operator (needed for range-based for) |
| `function `[`operator++`](#) |  |
| bool `function `[`operator==`](#) |  equality operator (needed for InputIterator) |
| bool `function `[`operator!=`](#) |  inequality operator (needed for range-based for) |
| const `function `[`key`](#) |  return key of the iterator |
| IteratorType::reference `function `[`value`](#) |  return value of the iterator |

## Members

### `difference_type`

**Type**: std::ptrdiff_t

---

### `value_type`

---

### `pointer`

---

### `reference`

---

### `iterator_category`

**Type**: std::forward_iterator_tag

---

### `string_type`

**Type**: typename std::remove_cv< typename std::remove_reference< decltype(std::declval< IteratorType >().

---

### `anchor`

**Type**: IteratorType

 the iterator

---

### `array_index`

**Type**: std::size_t

 an index for arrays (used to create key names)

---

### `array_index_last`

**Type**: std::size_t

 last stringified array index

---

### `array_index_str`

 a string representation of the array index

---

### `empty_str`

 an empty string (to return a reference for primitive values)

---

### `iteration_proxy_value`

---

### `iteration_proxy_value`

---

### `iteration_proxy_value`

---

### `operator=`

---

### `iteration_proxy_value`

---

### `operator=`

---

### `~iteration_proxy_value`

---

### `operator*`

**Type**: const

 dereference operator (needed for range-based for)

---

### `operator++`

 increment operator (needed for range-based for)

---

### `operator++`

---

### `operator==`

**Type**: bool

 equality operator (needed for InputIterator)

---

### `operator!=`

**Type**: bool

 inequality operator (needed for range-based for)

---

### `key`

**Type**: const

 return key of the iterator

---

### `value`

**Type**: IteratorType::reference

 return value of the iterator

---

