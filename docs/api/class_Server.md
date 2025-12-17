# class `httplib::Server`

## Summary

| Members | Descriptions |
|---------|--------------|
| `enum `[`HandlerResponse`](#) |  |
| std::function< void(const `typedef `[`Handler`](#) |  |
| std::function< void(const `typedef `[`ExceptionHandler`](#) |  |
| std::function< `typedef `[`HandlerWithResponse`](#) |  |
| std::function< void( const `typedef `[`HandlerWithContentReader`](#) |  |
| std::function< int(const `typedef `[`Expect100ContinueHandler`](#) |  |
| std::vector< std::pair< std::unique_ptr< `typedef `[`Handlers`](#) |  |
| std::vector< std::pair< std::unique_ptr< `typedef `[`HandlersForContentReader`](#) |  |
| std::function< `variable `[`new_task_queue`](#) |  |
| std::atomic< `variable `[`svr_sock_`](#) |  |
| std::vector< std::string > `variable `[`trusted_proxies_`](#) |  |
| size_t `variable `[`keep_alive_max_count_`](#) |  |
| time_t `variable `[`keep_alive_timeout_sec_`](#) |  |
| time_t `variable `[`read_timeout_sec_`](#) |  |
| time_t `variable `[`read_timeout_usec_`](#) |  |
| time_t `variable `[`write_timeout_sec_`](#) |  |
| time_t `variable `[`write_timeout_usec_`](#) |  |
| time_t `variable `[`idle_interval_sec_`](#) |  |
| time_t `variable `[`idle_interval_usec_`](#) |  |
| size_t `variable `[`payload_max_length_`](#) |  |
| std::atomic< bool > `variable `[`is_running_`](#) |  |
| std::atomic< bool > `variable `[`is_decommissioned`](#) |  |
| std::vector< `variable `[`base_dirs_`](#) |  |
| std::map< std::string, std::string > `variable `[`file_extension_and_mimetype_map_`](#) |  |
| std::string `variable `[`default_file_mimetype_`](#) |  |
| `variable `[`file_request_handler_`](#) |  |
| `variable `[`get_handlers_`](#) |  |
| `variable `[`post_handlers_`](#) |  |
| `variable `[`post_handlers_for_content_reader_`](#) |  |
| `variable `[`put_handlers_`](#) |  |
| `variable `[`put_handlers_for_content_reader_`](#) |  |
| `variable `[`patch_handlers_`](#) |  |
| `variable `[`patch_handlers_for_content_reader_`](#) |  |
| `variable `[`delete_handlers_`](#) |  |
| `variable `[`delete_handlers_for_content_reader_`](#) |  |
| `variable `[`options_handlers_`](#) |  |
| `variable `[`error_handler_`](#) |  |
| `variable `[`exception_handler_`](#) |  |
| `variable `[`pre_routing_handler_`](#) |  |
| `variable `[`post_routing_handler_`](#) |  |
| `variable `[`pre_request_handler_`](#) |  |
| `variable `[`expect_100_continue_handler_`](#) |  |
| std::mutex `variable `[`logger_mutex_`](#) |  |
| `variable `[`logger_`](#) |  |
| `variable `[`pre_compression_logger_`](#) |  |
| `variable `[`error_logger_`](#) |  |
| int `variable `[`address_family_`](#) |  |
| bool `variable `[`tcp_nodelay_`](#) |  |
| bool `variable `[`ipv6_v6only_`](#) |  |
| `variable `[`socket_options_`](#) |  |
| `variable `[`default_headers_`](#) |  |
| std::function< ssize_t( `variable `[`header_writer_`](#) |  |
| `function `[`Server`](#) |  |
| `function `[`~Server`](#) |  |
| bool `function `[`is_valid`](#) |  |
| `function `[`Get`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Post`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Put`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Patch`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Delete`](#) |  |
| `function `[`Options`](#) |  |
| bool `function `[`set_base_dir`](#) |  |
| bool `function `[`set_mount_point`](#) |  |
| bool `function `[`remove_mount_point`](#) |  |
| `function `[`set_file_extension_and_mimetype_mapping`](#) |  |
| `function `[`set_default_file_mimetype`](#) |  |
| `function `[`set_file_request_handler`](#) |  |
| `function `[`set_error_handler`](#) |  |
| `function `[`set_exception_handler`](#) |  |
| `function `[`set_pre_routing_handler`](#) |  |
| `function `[`set_post_routing_handler`](#) |  |
| `function `[`set_pre_request_handler`](#) |  |
| `function `[`set_expect_100_continue_handler`](#) |  |
| `function `[`set_logger`](#) |  |
| `function `[`set_pre_compression_logger`](#) |  |
| `function `[`set_error_logger`](#) |  |
| `function `[`set_address_family`](#) |  |
| `function `[`set_tcp_nodelay`](#) |  |
| `function `[`set_ipv6_v6only`](#) |  |
| `function `[`set_socket_options`](#) |  |
| `function `[`set_default_headers`](#) |  |
| `function `[`set_header_writer`](#) |  |
| `function `[`set_trusted_proxies`](#) |  |
| `function `[`set_keep_alive_max_count`](#) |  |
| `function `[`set_keep_alive_timeout`](#) |  |
| `function `[`set_read_timeout`](#) |  |
| `function `[`set_read_timeout`](#) |  |
| `function `[`set_write_timeout`](#) |  |
| `function `[`set_write_timeout`](#) |  |
| `function `[`set_idle_interval`](#) |  |
| `function `[`set_idle_interval`](#) |  |
| `function `[`set_payload_max_length`](#) |  |
| bool `function `[`bind_to_port`](#) |  |
| int `function `[`bind_to_any_port`](#) |  |
| bool `function `[`listen_after_bind`](#) |  |
| bool `function `[`listen`](#) |  |
| bool `function `[`is_running`](#) |  |
| void `function `[`wait_until_ready`](#) |  |
| void `function `[`stop`](#) |  |
| void `function `[`decommission`](#) |  |
| bool `function `[`process_request`](#) |  |
| std::unique_ptr< `function `[`make_matcher`](#) |  |
| `function `[`set_error_handler_core`](#) |  |
| `function `[`set_error_handler_core`](#) |  |
| `function `[`create_server_socket`](#) |  |
| int `function `[`bind_internal`](#) |  |
| bool `function `[`listen_internal`](#) |  |
| bool `function `[`routing`](#) |  |
| bool `function `[`handle_file_request`](#) |  |
| bool `function `[`dispatch_request`](#) |  |
| bool `function `[`dispatch_request_for_content_reader`](#) |  |
| bool `function `[`parse_request_line`](#) |  |
| void `function `[`apply_ranges`](#) |  |
| bool `function `[`write_response`](#) |  |
| bool `function `[`write_response_with_content`](#) |  |
| bool `function `[`write_response_core`](#) |  |
| bool `function `[`write_content_with_provider`](#) |  |
| bool `function `[`read_content`](#) |  |
| bool `function `[`read_content_with_content_receiver`](#) |  |
| bool `function `[`read_content_core`](#) |  |
| bool `function `[`process_and_close_socket`](#) |  |
| void `function `[`output_log`](#) |  |
| void `function `[`output_pre_compression_log`](#) |  |
| void `function `[`output_error_log`](#) |  |

## Members

### `HandlerResponse`

---

### `Handler`

**Type**: std::function< void(const

---

### `ExceptionHandler`

**Type**: std::function< void(const

---

### `HandlerWithResponse`

**Type**: std::function<

---

### `HandlerWithContentReader`

**Type**: std::function< void( const

---

### `Expect100ContinueHandler`

**Type**: std::function< int(const

---

### `Handlers`

**Type**: std::vector< std::pair< std::unique_ptr<

---

### `HandlersForContentReader`

**Type**: std::vector< std::pair< std::unique_ptr<

---

### `new_task_queue`

**Type**: std::function<

---

### `svr_sock_`

**Type**: std::atomic<

---

### `trusted_proxies_`

**Type**: std::vector< std::string >

---

### `keep_alive_max_count_`

**Type**: size_t

---

### `keep_alive_timeout_sec_`

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

### `idle_interval_sec_`

**Type**: time_t

---

### `idle_interval_usec_`

**Type**: time_t

---

### `payload_max_length_`

**Type**: size_t

---

### `is_running_`

**Type**: std::atomic< bool >

---

### `is_decommissioned`

**Type**: std::atomic< bool >

---

### `base_dirs_`

**Type**: std::vector<

---

### `file_extension_and_mimetype_map_`

**Type**: std::map< std::string, std::string >

---

### `default_file_mimetype_`

**Type**: std::string

---

### `file_request_handler_`

---

### `get_handlers_`

---

### `post_handlers_`

---

### `post_handlers_for_content_reader_`

---

### `put_handlers_`

---

### `put_handlers_for_content_reader_`

---

### `patch_handlers_`

---

### `patch_handlers_for_content_reader_`

---

### `delete_handlers_`

---

### `delete_handlers_for_content_reader_`

---

### `options_handlers_`

---

### `error_handler_`

---

### `exception_handler_`

---

### `pre_routing_handler_`

---

### `post_routing_handler_`

---

### `pre_request_handler_`

---

### `expect_100_continue_handler_`

---

### `logger_mutex_`

**Type**: std::mutex

---

### `logger_`

---

### `pre_compression_logger_`

---

### `error_logger_`

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

### `default_headers_`

---

### `header_writer_`

**Type**: std::function< ssize_t(

---

### `Server`

---

### `~Server`

---

### `is_valid`

**Type**: bool

---

### `Get`

---

### `Post`

---

### `Post`

---

### `Put`

---

### `Put`

---

### `Patch`

---

### `Patch`

---

### `Delete`

---

### `Delete`

---

### `Options`

---

### `set_base_dir`

**Type**: bool

---

### `set_mount_point`

**Type**: bool

---

### `remove_mount_point`

**Type**: bool

---

### `set_file_extension_and_mimetype_mapping`

---

### `set_default_file_mimetype`

---

### `set_file_request_handler`

---

### `set_error_handler`

---

### `set_exception_handler`

---

### `set_pre_routing_handler`

---

### `set_post_routing_handler`

---

### `set_pre_request_handler`

---

### `set_expect_100_continue_handler`

---

### `set_logger`

---

### `set_pre_compression_logger`

---

### `set_error_logger`

---

### `set_address_family`

---

### `set_tcp_nodelay`

---

### `set_ipv6_v6only`

---

### `set_socket_options`

---

### `set_default_headers`

---

### `set_header_writer`

---

### `set_trusted_proxies`

---

### `set_keep_alive_max_count`

---

### `set_keep_alive_timeout`

---

### `set_read_timeout`

---

### `set_read_timeout`

---

### `set_write_timeout`

---

### `set_write_timeout`

---

### `set_idle_interval`

---

### `set_idle_interval`

---

### `set_payload_max_length`

---

### `bind_to_port`

**Type**: bool

---

### `bind_to_any_port`

**Type**: int

---

### `listen_after_bind`

**Type**: bool

---

### `listen`

**Type**: bool

---

### `is_running`

**Type**: bool

---

### `wait_until_ready`

**Type**: void

---

### `stop`

**Type**: void

---

### `decommission`

**Type**: void

---

### `process_request`

**Type**: bool

---

### `make_matcher`

**Type**: std::unique_ptr<

---

### `set_error_handler_core`

---

### `set_error_handler_core`

---

### `create_server_socket`

---

### `bind_internal`

**Type**: int

---

### `listen_internal`

**Type**: bool

---

### `routing`

**Type**: bool

---

### `handle_file_request`

**Type**: bool

---

### `dispatch_request`

**Type**: bool

---

### `dispatch_request_for_content_reader`

**Type**: bool

---

### `parse_request_line`

**Type**: bool

---

### `apply_ranges`

**Type**: void

---

### `write_response`

**Type**: bool

---

### `write_response_with_content`

**Type**: bool

---

### `write_response_core`

**Type**: bool

---

### `write_content_with_provider`

**Type**: bool

---

### `read_content`

**Type**: bool

---

### `read_content_with_content_receiver`

**Type**: bool

---

### `read_content_core`

**Type**: bool

---

### `process_and_close_socket`

**Type**: bool

---

### `output_log`

**Type**: void

---

### `output_pre_compression_log`

**Type**: void

---

### `output_error_log`

**Type**: void

---

