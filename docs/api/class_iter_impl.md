# class `detail::iter_impl`

 a template for a bidirectional iterator for the

## Detailed Description

 An iterator is called version 1.0.0, simplified in version 2.0.9, change to bidirectional iterators in version 3.0.0 (see

## Summary

| Members | Descriptions |
|---------|--------------|
| `typedef `[`other_iter_impl`](#) |  |
| typename BasicJsonType::object_t `typedef `[`object_t`](#) |  |
| typename BasicJsonType::array_t `typedef `[`array_t`](#) |  |
| typename BasicJsonType::value_type `typedef `[`value_type`](#) |  |
| typename BasicJsonType::difference_type `typedef `[`difference_type`](#) |  |
| members friend `variable `[`other_iter_impl`](#) |  |
| friend `variable `[`BasicJsonType`](#) |  |
| friend `variable `[`iteration_proxy< iter_impl >`](#) |  |
| friend `variable `[`iteration_proxy_value< iter_impl >`](#) |  |
| `variable `[`__pad0__`](#) |  |
| `variable `[`__pad1__`](#) |  |
| *the actual `variable `[`m_it`](#) |  |
| *defines `function `[`over`](#) |  |
| *defines `function `[`over`](#) |  |
| `function `[`iter_impl`](#) |  |
| `function `[`~iter_impl`](#) |  |
| `function `[`iter_impl`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`iter_impl`](#) |  constructor for a given JSON instance |
| `function `[`iter_impl`](#) |  const copy constructor |
| `function `[`operator=`](#) |  converting assignment |
| `function `[`iter_impl`](#) |  converting constructor |
| `function `[`operator=`](#) |  converting assignment |
| `function `[`switch`](#) |  |
| void `function `[`set_end`](#) |  set the iterator past the last value |
| reference `function `[`operator*`](#) |  return a reference to the value pointed to by the iterator |
| pointer `function `[`operator->`](#) |  dereference the iterator |
| `function `[`operator++`](#) |  post-increment (it++) |
| `function `[`operator++`](#) |  pre-increment (++it) |
| `function `[`operator--`](#) |  post-decrement (it |
| `function `[`operator--`](#) |  pre-decrement ( |
| bool `function `[`operator==`](#) |  comparison: equal |
| bool `function `[`operator!=`](#) |  comparison: not equal |
| bool `function `[`operator<`](#) |  comparison: smaller |
| bool `function `[`operator<=`](#) |  comparison: less than or equal |
| bool `function `[`operator>`](#) |  comparison: greater than |
| bool `function `[`operator>=`](#) |  comparison: greater than or equal |
| `function `[`operator+=`](#) |  add to iterator |
| `function `[`operator-=`](#) |  subtract from iterator |
| `function `[`operator+`](#) |  add to iterator |
| `function `[`operator-`](#) |  subtract from iterator |
| `function `[`operator-`](#) |  return difference |
| reference `function `[`operator[]`](#) |  access to successor |
| const object_t::key_type & `function `[`key`](#) |  return the key of an object iterator |
| reference `function `[`value`](#) |  return the value of an iterator |
| `friend `[`operator+`](#) |  addition of distance and iterator |

## Members

### `other_iter_impl`

---

### `object_t`

**Type**: typename BasicJsonType::object_t

---

### `array_t`

**Type**: typename BasicJsonType::array_t

---

### `value_type`

**Type**: typename BasicJsonType::value_type

---

### `difference_type`

**Type**: typename BasicJsonType::difference_type

---

### `other_iter_impl`

**Type**: members friend

---

### `BasicJsonType`

**Type**: friend

---

### `iteration_proxy< iter_impl >`

**Type**: friend

---

### `iteration_proxy_value< iter_impl >`

**Type**: friend

---

### `__pad0__`

---

### `__pad1__`

---

### `m_it`

**Type**: *the actual

---

### `over`

**Type**: *defines

---

### `over`

**Type**: *defines

---

### `iter_impl`

---

### `~iter_impl`

---

### `iter_impl`

---

### `operator=`

---

### `iter_impl`

 constructor for a given JSON instance

---

### `iter_impl`

 const copy constructor

---

### `operator=`

 converting assignment

---

### `iter_impl`

 converting constructor

---

### `operator=`

 converting assignment

---

### `switch`

---

### `set_end`

**Type**: void

 set the iterator past the last value

---

### `operator*`

**Type**: reference

 return a reference to the value pointed to by the iterator

---

### `operator->`

**Type**: pointer

 dereference the iterator

---

### `operator++`

 post-increment (it++)

---

### `operator++`

 pre-increment (++it)

---

### `operator--`

 post-decrement (it

---

### `operator--`

 pre-decrement (

---

### `operator==`

**Type**: bool

 comparison: equal

---

### `operator!=`

**Type**: bool

 comparison: not equal

---

### `operator<`

**Type**: bool

 comparison: smaller

---

### `operator<=`

**Type**: bool

 comparison: less than or equal

---

### `operator>`

**Type**: bool

 comparison: greater than

---

### `operator>=`

**Type**: bool

 comparison: greater than or equal

---

### `operator+=`

 add to iterator

---

### `operator-=`

 subtract from iterator

---

### `operator+`

 add to iterator

---

### `operator-`

 subtract from iterator

---

### `operator-`

 return difference

---

### `operator[]`

**Type**: reference

 access to successor

---

### `key`

**Type**: const object_t::key_type &

 return the key of an object iterator

---

### `value`

**Type**: reference

 return the value of an iterator

---

### `operator+`

 addition of distance and iterator

---

