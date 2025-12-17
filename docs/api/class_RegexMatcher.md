# class `httplib::detail::RegexMatcher`

 Performs std::regex_match on request path and stores the result in

## Detailed Description

 Note that regex match is performed directly on the whole request. This means that wildcard patterns may match multiple path segments with /: "/begin/(.*)/end" will match both "/begin/middle/end" and "/begin/1/2/end".

## Summary

| Members | Descriptions |
|---------|--------------|
| std::regex `variable `[`regex_`](#) |  |
| `function `[`RegexMatcher`](#) |  |
| bool `function `[`match`](#) |  |

## Members

### `regex_`

**Type**: std::regex

---

### `RegexMatcher`

---

### `match`

**Type**: bool

---

