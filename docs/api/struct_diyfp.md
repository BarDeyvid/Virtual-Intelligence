# struct `detail::dtoa_impl::diyfp`

## Summary

| Members | Descriptions |
|---------|--------------|
| int `variable `[`kPrecision`](#) |  |
| std::uint64_t `variable `[`f`](#) |  |
| int `variable `[`e`](#) |  |
| constexpr `function `[`diyfp`](#) |  |
| `function `[`sub`](#) |  returns x - y |
| `function `[`mul`](#) |  returns x * y |
| `function `[`normalize`](#) |  normalize x such that the significand is >= 2^(q-1) |
| `function `[`normalize_to`](#) |  normalize x such that the result has the exponent E |

## Members

### `kPrecision`

**Type**: int

---

### `f`

**Type**: std::uint64_t

---

### `e`

**Type**: int

---

### `diyfp`

**Type**: constexpr

---

### `sub`

 returns x - y

---

### `mul`

 returns x * y

---

### `normalize`

 normalize x such that the significand is >= 2^(q-1)

---

### `normalize_to`

 normalize x such that the result has the exponent E

---

