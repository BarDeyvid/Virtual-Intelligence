# class `httplib::detail::SocketStream`

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`sock_`](#) |  |
| time_t `variable `[`read_timeout_sec_`](#) |  |
| time_t `variable `[`read_timeout_usec_`](#) |  |
| time_t `variable `[`write_timeout_sec_`](#) |  |
| time_t `variable `[`write_timeout_usec_`](#) |  |
| time_t `variable `[`max_timeout_msec_`](#) |  |
| const std::chrono::time_point< std::chrono::steady_clock > `variable `[`start_time_`](#) |  |
| std::vector< char > `variable `[`read_buff_`](#) |  |
| size_t `variable `[`read_buff_off_`](#) |  |
| size_t `variable `[`read_buff_content_size_`](#) |  |
| const size_t `variable `[`read_buff_size_`](#) |  |
| `function `[`SocketStream`](#) |  |
| `function `[`~SocketStream`](#) |  |
| bool `function `[`is_readable`](#) |  |
| bool `function `[`wait_readable`](#) |  |
| bool `function `[`wait_writable`](#) |  |
| ssize_t `function `[`read`](#) |  |
| ssize_t `function `[`write`](#) |  |
| void `function `[`get_remote_ip_and_port`](#) |  |
| void `function `[`get_local_ip_and_port`](#) |  |
| `function `[`socket`](#) |  |
| time_t `function `[`duration`](#) |  |

## Members

### `sock_`

---

### `read_timeout_sec_`

**Type**: time_t

---

### `read_timeout_usec_`

**Type**: time_t

---

### `write_timeout_sec_`

**Type**: time_t

---

### `write_timeout_usec_`

**Type**: time_t

---

### `max_timeout_msec_`

**Type**: time_t

---

### `start_time_`

**Type**: const std::chrono::time_point< std::chrono::steady_clock >

---

### `read_buff_`

**Type**: std::vector< char >

---

### `read_buff_off_`

**Type**: size_t

---

### `read_buff_content_size_`

**Type**: size_t

---

### `read_buff_size_`

**Type**: const size_t

---

### `SocketStream`

---

### `~SocketStream`

---

### `is_readable`

**Type**: bool

---

### `wait_readable`

**Type**: bool

---

### `wait_writable`

**Type**: bool

---

### `read`

**Type**: ssize_t

---

### `write`

**Type**: ssize_t

---

### `get_remote_ip_and_port`

**Type**: void

---

### `get_local_ip_and_port`

**Type**: void

---

### `socket`

---

### `duration`

**Type**: time_t

---

