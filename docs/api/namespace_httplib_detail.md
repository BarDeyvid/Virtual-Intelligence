# namespace `httplib::detail`

## Summary

| Members | Descriptions |
|---------|--------------|
| `enum `[`EncodingType`](#) |  |
| `enum `[`ReadContentResult`](#) |  |
| std::enable_if<!std::is_array< T >::value, std::unique_ptr< T > >::type `function `[`make_unique`](#) |  |
| std::enable_if< std::is_array< T >::value, std::unique_ptr< T > >::type `function `[`make_unique`](#) |  |
| bool `function `[`set_socket_opt_impl`](#) |  |
| bool `function `[`set_socket_opt`](#) |  |
| bool `function `[`set_socket_opt_time`](#) |  |
| ssize_t `function `[`write_headers`](#) |  |
| std::string `function `[`make_host_and_port_string`](#) |  |
| void `function `[`duration_to_sec_and_usec`](#) |  |
| size_t `function `[`str_len`](#) |  |
| bool `function `[`is_numeric`](#) |  |
| size_t `function `[`get_header_value_u64`](#) |  |
| size_t `function `[`get_header_value_u64`](#) |  |
| std::string `function `[`trim_copy`](#) |  |
| void `function `[`divide`](#) |  |
| void `function `[`divide`](#) |  |
| void `function `[`split`](#) |  |
| void `function `[`split`](#) |  |
| bool `function `[`process_client_socket`](#) |  |
| `function `[`create_client_socket`](#) |  |
| const char * `function `[`get_header_value`](#) |  |
| std::string `function `[`params_to_query_str`](#) |  |
| void `function `[`parse_query_text`](#) |  |
| void `function `[`parse_query_text`](#) |  |
| bool `function `[`parse_multipart_boundary`](#) |  |
| bool `function `[`parse_range_header`](#) |  |
| bool `function `[`parse_accept_header`](#) |  |
| int `function `[`close_socket`](#) |  |
| ssize_t `function `[`send_socket`](#) |  |
| ssize_t `function `[`read_socket`](#) |  |
| `function `[`encoding_type`](#) |  |
| bool `function `[`is_hex`](#) |  |
| bool `function `[`from_hex_to_i`](#) |  |
| std::string `function `[`from_i_to_hex`](#) |  |
| size_t `function `[`to_utf8`](#) |  |
| std::string `function `[`base64_encode`](#) |  |
| bool `function `[`is_valid_path`](#) |  |
| std::string `function `[`encode_path`](#) |  |
| std::string `function `[`file_extension`](#) |  |
| bool `function `[`is_space_or_tab`](#) |  |
| std::pair< size_t, size_t > `function `[`trim`](#) |  |
| std::string `function `[`trim_double_quotes_copy`](#) |  |
| ssize_t `function `[`handle_EINTR`](#) |  |
| int `function `[`poll_wrapper`](#) |  |
| ssize_t `function `[`select_impl`](#) |  |
| ssize_t `function `[`select_read`](#) |  |
| ssize_t `function `[`select_write`](#) |  |
| `function `[`wait_until_socket_is_ready`](#) |  |
| bool `function `[`is_socket_alive`](#) |  |
| bool `function `[`keep_alive`](#) |  |
| bool `function `[`process_server_socket_core`](#) |  |
| bool `function `[`process_server_socket`](#) |  |
| int `function `[`shutdown_socket`](#) |  |
| std::string `function `[`escape_abstract_namespace_unix_domain`](#) |  |
| std::string `function `[`unescape_abstract_namespace_unix_domain`](#) |  |
| int `function `[`getaddrinfo_with_timeout`](#) |  |
| `function `[`create_socket`](#) |  |
| void `function `[`set_nonblocking`](#) |  |
| bool `function `[`is_connection_error`](#) |  |
| bool `function `[`bind_ip_address`](#) |  |
| std::string `function `[`if2ip`](#) |  |
| bool `function `[`get_ip_and_port`](#) |  |
| void `function `[`get_local_ip_and_port`](#) |  |
| void `function `[`get_remote_ip_and_port`](#) |  |
| unsigned int `function `[`str2tag_core`](#) |  |
| unsigned int `function `[`str2tag`](#) |  |
| std::string `function `[`find_content_type`](#) |  |
| bool `function `[`can_compress_content_type`](#) |  |
| bool `function `[`is_prohibited_header_name`](#) |  |
| bool `function `[`has_header`](#) |  |
| bool `function `[`parse_header`](#) |  |
| bool `function `[`read_headers`](#) |  |
| bool `function `[`read_content_with_length`](#) |  |
| void `function `[`skip_content_with_length`](#) |  |
| `function `[`read_content_without_length`](#) |  |
| `function `[`read_content_chunked`](#) |  |
| bool `function `[`is_chunked_transfer_encoding`](#) |  |
| bool `function `[`prepare_content_receiver`](#) |  |
| bool `function `[`read_content`](#) |  |
| ssize_t `function `[`write_request_line`](#) |  |
| ssize_t `function `[`write_response_line`](#) |  |
| bool `function `[`write_data`](#) |  |
| bool `function `[`write_content_with_progress`](#) |  |
| bool `function `[`write_content`](#) |  |
| bool `function `[`write_content`](#) |  |
| bool `function `[`write_content_without_length`](#) |  |
| bool `function `[`write_content_chunked`](#) |  |
| bool `function `[`write_content_chunked`](#) |  |
| bool `function `[`redirect`](#) |  |
| void `function `[`parse_disposition_params`](#) |  |
| std::string `function `[`random_string`](#) |  |
| std::string `function `[`make_multipart_data_boundary`](#) |  |
| bool `function `[`is_multipart_boundary_chars_valid`](#) |  |
| std::string `function `[`serialize_multipart_formdata_item_begin`](#) |  |
| std::string `function `[`serialize_multipart_formdata_item_end`](#) |  |
| std::string `function `[`serialize_multipart_formdata_finish`](#) |  |
| std::string `function `[`serialize_multipart_formdata_get_content_type`](#) |  |
| std::string `function `[`serialize_multipart_formdata`](#) |  |
| void `function `[`coalesce_ranges`](#) |  |
| bool `function `[`range_error`](#) |  |
| std::pair< size_t, size_t > `function `[`get_range_offset_and_length`](#) |  |
| std::string `function `[`make_content_range_header_field`](#) |  |
| bool `function `[`process_multipart_ranges_data`](#) |  |
| void `function `[`make_multipart_ranges_data`](#) |  |
| size_t `function `[`get_multipart_ranges_data_length`](#) |  |
| bool `function `[`write_multipart_ranges_data`](#) |  |
| bool `function `[`expect_content`](#) |  |
| bool `function `[`has_crlf`](#) |  |
| bool `function `[`parse_www_authenticate`](#) |  |
| void `function `[`calc_actual_timeout`](#) |  |

## Members

### `EncodingType`

---

### `ReadContentResult`

---

### `make_unique`

**Type**: std::enable_if<!std::is_array< T >::value, std::unique_ptr< T > >::type

---

### `make_unique`

**Type**: std::enable_if< std::is_array< T >::value, std::unique_ptr< T > >::type

---

### `set_socket_opt_impl`

**Type**: bool

---

### `set_socket_opt`

**Type**: bool

---

### `set_socket_opt_time`

**Type**: bool

---

### `write_headers`

**Type**: ssize_t

---

### `make_host_and_port_string`

**Type**: std::string

---

### `duration_to_sec_and_usec`

**Type**: void

---

### `str_len`

**Type**: size_t

---

### `is_numeric`

**Type**: bool

---

### `get_header_value_u64`

**Type**: size_t

---

### `get_header_value_u64`

**Type**: size_t

---

### `trim_copy`

**Type**: std::string

---

### `divide`

**Type**: void

---

### `divide`

**Type**: void

---

### `split`

**Type**: void

---

### `split`

**Type**: void

---

### `process_client_socket`

**Type**: bool

---

### `create_client_socket`

---

### `get_header_value`

**Type**: const char *

---

### `params_to_query_str`

**Type**: std::string

---

### `parse_query_text`

**Type**: void

---

### `parse_query_text`

**Type**: void

---

### `parse_multipart_boundary`

**Type**: bool

---

### `parse_range_header`

**Type**: bool

---

### `parse_accept_header`

**Type**: bool

---

### `close_socket`

**Type**: int

---

### `send_socket`

**Type**: ssize_t

---

### `read_socket`

**Type**: ssize_t

---

### `encoding_type`

---

### `is_hex`

**Type**: bool

---

### `from_hex_to_i`

**Type**: bool

---

### `from_i_to_hex`

**Type**: std::string

---

### `to_utf8`

**Type**: size_t

---

### `base64_encode`

**Type**: std::string

---

### `is_valid_path`

**Type**: bool

---

### `encode_path`

**Type**: std::string

---

### `file_extension`

**Type**: std::string

---

### `is_space_or_tab`

**Type**: bool

---

### `trim`

**Type**: std::pair< size_t, size_t >

---

### `trim_double_quotes_copy`

**Type**: std::string

---

### `handle_EINTR`

**Type**: ssize_t

---

### `poll_wrapper`

**Type**: int

---

### `select_impl`

**Type**: ssize_t

---

### `select_read`

**Type**: ssize_t

---

### `select_write`

**Type**: ssize_t

---

### `wait_until_socket_is_ready`

---

### `is_socket_alive`

**Type**: bool

---

### `keep_alive`

**Type**: bool

---

### `process_server_socket_core`

**Type**: bool

---

### `process_server_socket`

**Type**: bool

---

### `shutdown_socket`

**Type**: int

---

### `escape_abstract_namespace_unix_domain`

**Type**: std::string

---

### `unescape_abstract_namespace_unix_domain`

**Type**: std::string

---

### `getaddrinfo_with_timeout`

**Type**: int

---

### `create_socket`

---

### `set_nonblocking`

**Type**: void

---

### `is_connection_error`

**Type**: bool

---

### `bind_ip_address`

**Type**: bool

---

### `if2ip`

**Type**: std::string

---

### `get_ip_and_port`

**Type**: bool

---

### `get_local_ip_and_port`

**Type**: void

---

### `get_remote_ip_and_port`

**Type**: void

---

### `str2tag_core`

**Type**: unsigned int

---

### `str2tag`

**Type**: unsigned int

---

### `find_content_type`

**Type**: std::string

---

### `can_compress_content_type`

**Type**: bool

---

### `is_prohibited_header_name`

**Type**: bool

---

### `has_header`

**Type**: bool

---

### `parse_header`

**Type**: bool

---

### `read_headers`

**Type**: bool

---

### `read_content_with_length`

**Type**: bool

---

### `skip_content_with_length`

**Type**: void

---

### `read_content_without_length`

---

### `read_content_chunked`

---

### `is_chunked_transfer_encoding`

**Type**: bool

---

### `prepare_content_receiver`

**Type**: bool

---

### `read_content`

**Type**: bool

---

### `write_request_line`

**Type**: ssize_t

---

### `write_response_line`

**Type**: ssize_t

---

### `write_data`

**Type**: bool

---

### `write_content_with_progress`

**Type**: bool

---

### `write_content`

**Type**: bool

---

### `write_content`

**Type**: bool

---

### `write_content_without_length`

**Type**: bool

---

### `write_content_chunked`

**Type**: bool

---

### `write_content_chunked`

**Type**: bool

---

### `redirect`

**Type**: bool

---

### `parse_disposition_params`

**Type**: void

---

### `random_string`

**Type**: std::string

---

### `make_multipart_data_boundary`

**Type**: std::string

---

### `is_multipart_boundary_chars_valid`

**Type**: bool

---

### `serialize_multipart_formdata_item_begin`

**Type**: std::string

---

### `serialize_multipart_formdata_item_end`

**Type**: std::string

---

### `serialize_multipart_formdata_finish`

**Type**: std::string

---

### `serialize_multipart_formdata_get_content_type`

**Type**: std::string

---

### `serialize_multipart_formdata`

**Type**: std::string

---

### `coalesce_ranges`

**Type**: void

---

### `range_error`

**Type**: bool

---

### `get_range_offset_and_length`

**Type**: std::pair< size_t, size_t >

---

### `make_content_range_header_field`

**Type**: std::string

---

### `process_multipart_ranges_data`

**Type**: bool

---

### `make_multipart_ranges_data`

**Type**: void

---

### `get_multipart_ranges_data_length`

**Type**: size_t

---

### `write_multipart_ranges_data`

**Type**: bool

---

### `expect_content`

**Type**: bool

---

### `has_crlf`

**Type**: bool

---

### `parse_www_authenticate`

**Type**: bool

---

### `calc_actual_timeout`

**Type**: void

---

