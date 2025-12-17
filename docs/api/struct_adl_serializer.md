# struct `adl_serializer`

 namespace for Niels Lohmann

## Detailed Description

 version 1.0.0 This serializer ignores the template arguments and uses ADL (

## Summary

| Members | Descriptions |
|---------|--------------|
| decltype(::nlohmann::from_json(std::forward< BasicJsonType >(j), val), void()) `function `[`from_json`](#) |  convert a JSON value to any value type |
| decltype(::nlohmann::from_json(std::forward< BasicJsonType >(j), `function `[`from_json`](#) |  convert a JSON value to any value type |
| decltype(::nlohmann::to_json(j, std::forward< TargetType >(val)), void()) `function `[`to_json`](#) |  convert any value type to a JSON value |

## Members

### `from_json`

**Type**: decltype(::nlohmann::from_json(std::forward< BasicJsonType >(j), val), void())

 convert a JSON value to any value type

---

### `from_json`

**Type**: decltype(::nlohmann::from_json(std::forward< BasicJsonType >(j),

 convert a JSON value to any value type

---

### `to_json`

**Type**: decltype(::nlohmann::to_json(j, std::forward< TargetType >(val)), void())

 convert any value type to a JSON value

---

