# struct `httplib::Request`

## Summary

| Members | Descriptions |
|---------|--------------|
| std::string `variable `[`method`](#) |  |
| std::string `variable `[`path`](#) |  |
| std::string `variable `[`matched_route`](#) |  |
| `variable `[`params`](#) |  |
| `variable `[`headers`](#) |  |
| `variable `[`trailers`](#) |  |
| std::string `variable `[`body`](#) |  |
| std::string `variable `[`remote_addr`](#) |  |
| int `variable `[`remote_port`](#) |  |
| std::string `variable `[`local_addr`](#) |  |
| int `variable `[`local_port`](#) |  |
| std::string `variable `[`version`](#) |  |
| std::string `variable `[`target`](#) |  |
| `variable `[`form`](#) |  |
| `variable `[`ranges`](#) |  |
| `variable `[`matches`](#) |  |
| std::unordered_map< std::string, std::string > `variable `[`path_params`](#) |  |
| std::function< bool()> `variable `[`is_connection_closed`](#) |  |
| std::vector< std::string > `variable `[`accept_content_types`](#) |  |
| `variable `[`response_handler`](#) |  |
| `variable `[`content_receiver`](#) |  |
| `variable `[`download_progress`](#) |  |
| `variable `[`upload_progress`](#) |  |
| size_t `variable `[`redirect_count_`](#) |  |
| size_t `variable `[`content_length_`](#) |  |
| `variable `[`content_provider_`](#) |  |
| bool `variable `[`is_chunked_content_provider_`](#) |  |
| size_t `variable `[`authorization_count_`](#) |  |
| std::chrono::time_point< std::chrono::steady_clock > `variable `[`start_time_`](#) |  |
| bool `function `[`has_header`](#) |  |
| std::string `function `[`get_header_value`](#) |  |
| size_t `function `[`get_header_value_u64`](#) |  |
| size_t `function `[`get_header_value_count`](#) |  |
| void `function `[`set_header`](#) |  |
| bool `function `[`has_trailer`](#) |  |
| std::string `function `[`get_trailer_value`](#) |  |
| size_t `function `[`get_trailer_value_count`](#) |  |
| bool `function `[`has_param`](#) |  |
| std::string `function `[`get_param_value`](#) |  |
| size_t `function `[`get_param_value_count`](#) |  |
| bool `function `[`is_multipart_form_data`](#) |  |

## Members

### `method`

**Type**: std::string

---

### `path`

**Type**: std::string

---

### `matched_route`

**Type**: std::string

---

### `params`

---

### `headers`

---

### `trailers`

---

### `body`

**Type**: std::string

---

### `remote_addr`

**Type**: std::string

---

### `remote_port`

**Type**: int

---

### `local_addr`

**Type**: std::string

---

### `local_port`

**Type**: int

---

### `version`

**Type**: std::string

---

### `target`

**Type**: std::string

---

### `form`

---

### `ranges`

---

### `matches`

---

### `path_params`

**Type**: std::unordered_map< std::string, std::string >

---

### `is_connection_closed`

**Type**: std::function< bool()>

---

### `accept_content_types`

**Type**: std::vector< std::string >

---

### `response_handler`

---

### `content_receiver`

---

### `download_progress`

---

### `upload_progress`

---

### `redirect_count_`

**Type**: size_t

---

### `content_length_`

**Type**: size_t

---

### `content_provider_`

---

### `is_chunked_content_provider_`

**Type**: bool

---

### `authorization_count_`

**Type**: size_t

---

### `start_time_`

**Type**: std::chrono::time_point< std::chrono::steady_clock >

---

### `has_header`

**Type**: bool

---

### `get_header_value`

**Type**: std::string

---

### `get_header_value_u64`

**Type**: size_t

---

### `get_header_value_count`

**Type**: size_t

---

### `set_header`

**Type**: void

---

### `has_trailer`

**Type**: bool

---

### `get_trailer_value`

**Type**: std::string

---

### `get_trailer_value_count`

**Type**: size_t

---

### `has_param`

**Type**: bool

---

### `get_param_value`

**Type**: std::string

---

### `get_param_value_count`

**Type**: size_t

---

### `is_multipart_form_data`

**Type**: bool

---

