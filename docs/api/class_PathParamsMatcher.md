# class `httplib::detail::PathParamsMatcher`

 Captures parameters in request path and stores them in

## Detailed Description

 Capture name is a substring of a pattern from : to /. The rest of the pattern is matched against the request path directly Parameters are captured starting from the next character after the end of the last matched static pattern fragment until the next /. Example pattern: "/path/fragments/:capture/more/fragments/:second_capture" Static fragments: "/path/fragments/", "more/fragments/" Given the following request path: "/path/fragments/:1/more/fragments/:2" the resulting capture will be {{"capture", "1"}, {"second_capture", "2"}}

## Summary

| Members | Descriptions |
|---------|--------------|
| char `variable `[`separator`](#) |  |
| std::vector< std::string > `variable `[`static_fragments_`](#) |  |
| std::vector< std::string > `variable `[`param_names_`](#) |  |
| `function `[`PathParamsMatcher`](#) |  |
| bool `function `[`match`](#) |  |

## Members

### `separator`

**Type**: char

---

### `static_fragments_`

**Type**: std::vector< std::string >

---

### `param_names_`

**Type**: std::vector< std::string >

---

### `PathParamsMatcher`

---

### `match`

**Type**: bool

---

