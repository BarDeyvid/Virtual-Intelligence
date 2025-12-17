# namespace `detail::dtoa_impl`

 implements the Grisu2 algorithm for binary to decimal floating-point conversion.

## Detailed Description

 This implementation is a slightly modified version of the reference implementation which may be obtained from The code is distributed under the MIT license, Copyright (c) 2009 Florian Loitsch. For a detailed description of the algorithm see: [1] Loitsch, "Printing Floating-Point Numbers Quickly and Accurately with
    Integers", Proceedings of the ACM SIGPLAN 2010 Conference on Programming Language Design and Implementation, PLDI 2010 [2] Burger, Dybvig, "Printing Floating-Point Numbers Quickly and Accurately", Proceedings of the ACM SIGPLAN 1996 Conference on Programming Language Design and Implementation, PLDI 1996

## Summary

| Members | Descriptions |
|---------|--------------|
| int `variable `[`kAlpha`](#) |  |
| int `variable `[`kGamma`](#) |  |
| Target `function `[`reinterpret_bits`](#) |  |
| `function `[`compute_boundaries`](#) |  |
| `function `[`get_cached_power_for_binary_exponent`](#) |  |
| int `function `[`find_largest_pow10`](#) |  |
| void `function `[`grisu2_round`](#) |  |
| void `function `[`grisu2_digit_gen`](#) |  |
| void `function `[`grisu2`](#) |  |
| void `function `[`grisu2`](#) |  |
| `function `[`append_exponent`](#) |  appends a decimal representation of e to buf |
| `function `[`format_buffer`](#) |  prettify v = buf * 10^decimal_exponent |

## Members

### `kAlpha`

**Type**: int

---

### `kGamma`

**Type**: int

---

### `reinterpret_bits`

**Type**: Target

---

### `compute_boundaries`

---

### `get_cached_power_for_binary_exponent`

---

### `find_largest_pow10`

**Type**: int

---

### `grisu2_round`

**Type**: void

---

### `grisu2_digit_gen`

**Type**: void

---

### `grisu2`

**Type**: void

---

### `grisu2`

**Type**: void

---

### `append_exponent`

 appends a decimal representation of e to buf

---

### `format_buffer`

 prettify v = buf * 10^decimal_exponent

---

