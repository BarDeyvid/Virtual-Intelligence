# class `httplib::ClientImpl`

## Summary

| Members | Descriptions |
|---------|--------------|
| const std::string `variable `[`host_`](#) |  |
| const int `variable `[`port_`](#) |  |
| const std::string `variable `[`host_and_port_`](#) |  |
| `variable `[`socket_`](#) |  |
| std::mutex `variable `[`socket_mutex_`](#) |  |
| std::recursive_mutex `variable `[`request_mutex_`](#) |  |
| size_t `variable `[`socket_requests_in_flight_`](#) |  |
| std::thread::id `variable `[`socket_requests_are_from_thread_`](#) |  |
| bool `variable `[`socket_should_be_closed_when_request_is_done_`](#) |  |
| std::map< std::string, std::string > `variable `[`addr_map_`](#) |  |
| `variable `[`default_headers_`](#) |  |
| std::function< ssize_t( `variable `[`header_writer_`](#) |  |
| std::string `variable `[`client_cert_path_`](#) |  |
| std::string `variable `[`client_key_path_`](#) |  |
| time_t `variable `[`connection_timeout_sec_`](#) |  |
| time_t `variable `[`connection_timeout_usec_`](#) |  |
| time_t `variable `[`read_timeout_sec_`](#) |  |
| time_t `variable `[`read_timeout_usec_`](#) |  |
| time_t `variable `[`write_timeout_sec_`](#) |  |
| time_t `variable `[`write_timeout_usec_`](#) |  |
| time_t `variable `[`max_timeout_msec_`](#) |  |
| std::string `variable `[`basic_auth_username_`](#) |  |
| std::string `variable `[`basic_auth_password_`](#) |  |
| std::string `variable `[`bearer_token_auth_token_`](#) |  |
| bool `variable `[`keep_alive_`](#) |  |
| bool `variable `[`follow_location_`](#) |  |
| bool `variable `[`path_encode_`](#) |  |
| int `variable `[`address_family_`](#) |  |
| bool `variable `[`tcp_nodelay_`](#) |  |
| bool `variable `[`ipv6_v6only_`](#) |  |
| `variable `[`socket_options_`](#) |  |
| bool `variable `[`compress_`](#) |  |
| bool `variable `[`decompress_`](#) |  |
| std::string `variable `[`interface_`](#) |  |
| std::string `variable `[`proxy_host_`](#) |  |
| int `variable `[`proxy_port_`](#) |  |
| std::string `variable `[`proxy_basic_auth_username_`](#) |  |
| std::string `variable `[`proxy_basic_auth_password_`](#) |  |
| std::string `variable `[`proxy_bearer_token_auth_token_`](#) |  |
| std::mutex `variable `[`logger_mutex_`](#) |  |
| `variable `[`logger_`](#) |  |
| `variable `[`error_logger_`](#) |  |
| `function `[`ClientImpl`](#) |  |
| `function `[`ClientImpl`](#) |  |
| `function `[`ClientImpl`](#) |  |
| `function `[`~ClientImpl`](#) |  |
| bool `function `[`is_valid`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Head`](#) |  |
| `function `[`Head`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Options`](#) |  |
| `function `[`Options`](#) |  |
| bool `function `[`send`](#) |  |
| `function `[`send`](#) |  |
| void `function `[`stop`](#) |  |
| std::string `function `[`host`](#) |  |
| int `function `[`port`](#) |  |
| size_t `function `[`is_socket_open`](#) |  |
| `function `[`socket`](#) |  |
| void `function `[`set_hostname_addr_map`](#) |  |
| void `function `[`set_default_headers`](#) |  |
| void `function `[`set_header_writer`](#) |  |
| void `function `[`set_address_family`](#) |  |
| void `function `[`set_tcp_nodelay`](#) |  |
| void `function `[`set_ipv6_v6only`](#) |  |
| void `function `[`set_socket_options`](#) |  |
| void `function `[`set_connection_timeout`](#) |  |
| void `function `[`set_connection_timeout`](#) |  |
| void `function `[`set_read_timeout`](#) |  |
| void `function `[`set_read_timeout`](#) |  |
| void `function `[`set_write_timeout`](#) |  |
| void `function `[`set_write_timeout`](#) |  |
| void `function `[`set_max_timeout`](#) |  |
| void `function `[`set_max_timeout`](#) |  |
| void `function `[`set_basic_auth`](#) |  |
| void `function `[`set_bearer_token_auth`](#) |  |
| void `function `[`set_keep_alive`](#) |  |
| void `function `[`set_follow_location`](#) |  |
| void `function `[`set_path_encode`](#) |  |
| void `function `[`set_compress`](#) |  |
| void `function `[`set_decompress`](#) |  |
| void `function `[`set_interface`](#) |  |
| void `function `[`set_proxy`](#) |  |
| void `function `[`set_proxy_basic_auth`](#) |  |
| void `function `[`set_proxy_bearer_token_auth`](#) |  |
| void `function `[`set_logger`](#) |  |
| void `function `[`set_error_logger`](#) |  |
| bool `function `[`create_and_connect_socket`](#) |  |
| void `function `[`shutdown_ssl`](#) |  |
| void `function `[`shutdown_socket`](#) |  |
| void `function `[`close_socket`](#) |  |
| bool `function `[`process_request`](#) |  |
| bool `function `[`write_content_with_provider`](#) |  |
| void `function `[`copy_settings`](#) |  |
| void `function `[`output_log`](#) |  |
| void `function `[`output_error_log`](#) |  |
| bool `function `[`send_`](#) |  |
| `function `[`send_`](#) |  |
| `function `[`create_client_socket`](#) |  |
| bool `function `[`read_response_line`](#) |  |
| bool `function `[`write_request`](#) |  |
| bool `function `[`redirect`](#) |  |
| bool `function `[`create_redirect_client`](#) |  |
| void `function `[`setup_redirect_client`](#) |  |
| bool `function `[`handle_request`](#) |  |
| std::unique_ptr< `function `[`send_with_content_provider`](#) |  |
| `function `[`send_with_content_provider`](#) |  |
| `function `[`get_multipart_content_provider`](#) |  |
| bool `function `[`process_socket`](#) |  |
| bool `function `[`is_ssl`](#) |  |

## Members

### `host_`

**Type**: const std::string

---

### `port_`

**Type**: const int

---

### `host_and_port_`

**Type**: const std::string

---

### `socket_`

---

### `socket_mutex_`

**Type**: std::mutex

---

### `request_mutex_`

**Type**: std::recursive_mutex

---

### `socket_requests_in_flight_`

**Type**: size_t

---

### `socket_requests_are_from_thread_`

**Type**: std::thread::id

---

### `socket_should_be_closed_when_request_is_done_`

**Type**: bool

---

### `addr_map_`

**Type**: std::map< std::string, std::string >

---

### `default_headers_`

---

### `header_writer_`

**Type**: std::function< ssize_t(

---

### `client_cert_path_`

**Type**: std::string

---

### `client_key_path_`

**Type**: std::string

---

### `connection_timeout_sec_`

**Type**: time_t

---

### `connection_timeout_usec_`

**Type**: time_t

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

### `basic_auth_username_`

**Type**: std::string

---

### `basic_auth_password_`

**Type**: std::string

---

### `bearer_token_auth_token_`

**Type**: std::string

---

### `keep_alive_`

**Type**: bool

---

### `follow_location_`

**Type**: bool

---

### `path_encode_`

**Type**: bool

---

### `address_family_`

**Type**: int

---

### `tcp_nodelay_`

**Type**: bool

---

### `ipv6_v6only_`

**Type**: bool

---

### `socket_options_`

---

### `compress_`

**Type**: bool

---

### `decompress_`

**Type**: bool

---

### `interface_`

**Type**: std::string

---

### `proxy_host_`

**Type**: std::string

---

### `proxy_port_`

**Type**: int

---

### `proxy_basic_auth_username_`

**Type**: std::string

---

### `proxy_basic_auth_password_`

**Type**: std::string

---

### `proxy_bearer_token_auth_token_`

**Type**: std::string

---

### `logger_mutex_`

**Type**: std::mutex

---

### `logger_`

---

### `error_logger_`

---

### `ClientImpl`

---

### `ClientImpl`

---

### `ClientImpl`

---

### `~ClientImpl`

---

### `is_valid`

**Type**: bool

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Get`

---

### `Head`

---

### `Head`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Post`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Put`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Patch`

---

### `Delete`

---

### `Delete`

---

### `Delete`

---

### `Delete`

---

### `Delete`

---

### `Delete`

---

### `Delete`

---

### `Delete`

---

### `Options`

---

### `Options`

---

### `send`

**Type**: bool

---

### `send`

---

### `stop`

**Type**: void

---

### `host`

**Type**: std::string

---

### `port`

**Type**: int

---

### `is_socket_open`

**Type**: size_t

---

### `socket`

---

### `set_hostname_addr_map`

**Type**: void

---

### `set_default_headers`

**Type**: void

---

### `set_header_writer`

**Type**: void

---

### `set_address_family`

**Type**: void

---

### `set_tcp_nodelay`

**Type**: void

---

### `set_ipv6_v6only`

**Type**: void

---

### `set_socket_options`

**Type**: void

---

### `set_connection_timeout`

**Type**: void

---

### `set_connection_timeout`

**Type**: void

---

### `set_read_timeout`

**Type**: void

---

### `set_read_timeout`

**Type**: void

---

### `set_write_timeout`

**Type**: void

---

### `set_write_timeout`

**Type**: void

---

### `set_max_timeout`

**Type**: void

---

### `set_max_timeout`

**Type**: void

---

### `set_basic_auth`

**Type**: void

---

### `set_bearer_token_auth`

**Type**: void

---

### `set_keep_alive`

**Type**: void

---

### `set_follow_location`

**Type**: void

---

### `set_path_encode`

**Type**: void

---

### `set_compress`

**Type**: void

---

### `set_decompress`

**Type**: void

---

### `set_interface`

**Type**: void

---

### `set_proxy`

**Type**: void

---

### `set_proxy_basic_auth`

**Type**: void

---

### `set_proxy_bearer_token_auth`

**Type**: void

---

### `set_logger`

**Type**: void

---

### `set_error_logger`

**Type**: void

---

### `create_and_connect_socket`

**Type**: bool

---

### `shutdown_ssl`

**Type**: void

---

### `shutdown_socket`

**Type**: void

---

### `close_socket`

**Type**: void

---

### `process_request`

**Type**: bool

---

### `write_content_with_provider`

**Type**: bool

---

### `copy_settings`

**Type**: void

---

### `output_log`

**Type**: void

---

### `output_error_log`

**Type**: void

---

### `send_`

**Type**: bool

---

### `send_`

---

### `create_client_socket`

---

### `read_response_line`

**Type**: bool

---

### `write_request`

**Type**: bool

---

### `redirect`

**Type**: bool

---

### `create_redirect_client`

**Type**: bool

---

### `setup_redirect_client`

**Type**: void

---

### `handle_request`

**Type**: bool

---

### `send_with_content_provider`

**Type**: std::unique_ptr<

---

### `send_with_content_provider`

---

### `get_multipart_content_provider`

---

### `process_socket`

**Type**: bool

---

### `is_ssl`

**Type**: bool

---

