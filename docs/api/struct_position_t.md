# struct `detail::position_t`

 struct to capture the start position of the current token

## Summary

| Members | Descriptions |
|---------|--------------|
| std::size_t `variable `[`chars_read_total`](#) |  the total number of characters read |
| std::size_t `variable `[`chars_read_current_line`](#) |  the number of characters read in the current line |
| std::size_t `variable `[`lines_read`](#) |  the number of lines read |
| constexpr `function `[`operator size_t`](#) |  conversion to size_t to preserve SAX interface |

## Members

### `chars_read_total`

**Type**: std::size_t

 the total number of characters read

---

### `chars_read_current_line`

**Type**: std::size_t

 the number of characters read in the current line

---

### `lines_read`

**Type**: std::size_t

 the number of lines read

---

### `operator size_t`

**Type**: constexpr

 conversion to size_t to preserve SAX interface

---

