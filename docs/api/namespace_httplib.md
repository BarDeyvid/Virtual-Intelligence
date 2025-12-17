# namespace `httplib`

## Summary

| Members | Descriptions |
|---------|--------------|
| `enum `[`SSLVerifierResponse`](#) |  |
| `enum `[`StatusCode`](#) |  |
| `enum `[`Error`](#) |  |
| std::unordered_multimap< std::string, std::string, `typedef `[`Headers`](#) |  |
| std::multimap< std::string, std::string > `typedef `[`Params`](#) |  |
| std::smatch `typedef `[`Match`](#) |  |
| std::function< bool(size_t current, size_t total)> `typedef `[`DownloadProgress`](#) |  |
| std::function< bool(size_t current, size_t total)> `typedef `[`UploadProgress`](#) |  |
| std::function< bool(const `typedef `[`ResponseHandler`](#) |  |
| std::multimap< std::string, `typedef `[`FormFields`](#) |  |
| std::multimap< std::string, `typedef `[`FormFiles`](#) |  |
| std::vector< `typedef `[`UploadFormDataItems`](#) |  |
| std::function< bool(size_t offset, size_t length, `typedef `[`ContentProvider`](#) |  |
| std::function< bool(size_t offset, `typedef `[`ContentProviderWithoutLength`](#) |  |
| std::function< void(bool success)> `typedef `[`ContentProviderResourceReleaser`](#) |  |
| std::vector< `typedef `[`FormDataProviderItems`](#) |  |
| std::function< bool( const char *data, size_t data_length, size_t offset, size_t total_length)> `typedef `[`ContentReceiverWithProgress`](#) |  |
| std::function< bool(const char *data, size_t data_length)> `typedef `[`ContentReceiver`](#) |  |
| std::function< bool(const `typedef `[`FormDataHeader`](#) |  |
| std::pair< ssize_t, ssize_t > `typedef `[`Range`](#) |  |
| std::vector< `typedef `[`Ranges`](#) |  |
| std::function< void(const `typedef `[`Logger`](#) |  |
| std::function< void(const `typedef `[`ErrorLogger`](#) |  |
| std::function< void( `typedef `[`SocketOptions`](#) |  |
| void `function `[`default_socket_options`](#) |  |
| const char * `function `[`status_message`](#) |  |
| std::string `function `[`get_bearer_token_auth`](#) |  |
| std::string `function `[`to_string`](#) |  |
| std::ostream & `function `[`operator<<`](#) |  |
| std::string `function `[`hosted_at`](#) |  |
| void `function `[`hosted_at`](#) |  |
| std::string `function `[`encode_uri_component`](#) |  |
| std::string `function `[`encode_uri`](#) |  |
| std::string `function `[`decode_uri_component`](#) |  |
| std::string `function `[`decode_uri`](#) |  |
| std::string `function `[`encode_path_component`](#) |  |
| std::string `function `[`decode_path_component`](#) |  |
| std::string `function `[`encode_query_component`](#) |  |
| std::string `function `[`decode_query_component`](#) |  |
| std::string `function `[`append_query_params`](#) |  |
| std::pair< std::string, std::string > `function `[`make_range_header`](#) |  |
| std::pair< std::string, std::string > `function `[`make_basic_authentication_header`](#) |  |
| std::pair< std::string, std::string > `function `[`make_bearer_token_authentication_header`](#) |  |
| std::string `function `[`get_client_ip`](#) |  |

## Members

### `SSLVerifierResponse`

---

### `StatusCode`

---

### `Error`

---

### `Headers`

**Type**: std::unordered_multimap< std::string, std::string,

---

### `Params`

**Type**: std::multimap< std::string, std::string >

---

### `Match`

**Type**: std::smatch

---

### `DownloadProgress`

**Type**: std::function< bool(size_t current, size_t total)>

---

### `UploadProgress`

**Type**: std::function< bool(size_t current, size_t total)>

---

### `ResponseHandler`

**Type**: std::function< bool(const

---

### `FormFields`

**Type**: std::multimap< std::string,

---

### `FormFiles`

**Type**: std::multimap< std::string,

---

### `UploadFormDataItems`

**Type**: std::vector<

---

### `ContentProvider`

**Type**: std::function< bool(size_t offset, size_t length,

---

### `ContentProviderWithoutLength`

**Type**: std::function< bool(size_t offset,

---

### `ContentProviderResourceReleaser`

**Type**: std::function< void(bool success)>

---

### `FormDataProviderItems`

**Type**: std::vector<

---

### `ContentReceiverWithProgress`

**Type**: std::function< bool( const char *data, size_t data_length, size_t offset, size_t total_length)>

---

### `ContentReceiver`

**Type**: std::function< bool(const char *data, size_t data_length)>

---

### `FormDataHeader`

**Type**: std::function< bool(const

---

### `Range`

**Type**: std::pair< ssize_t, ssize_t >

---

### `Ranges`

**Type**: std::vector<

---

### `Logger`

**Type**: std::function< void(const

---

### `ErrorLogger`

**Type**: std::function< void(const

---

### `SocketOptions`

**Type**: std::function< void(

---

### `default_socket_options`

**Type**: void

---

### `status_message`

**Type**: const char *

---

### `get_bearer_token_auth`

**Type**: std::string

---

### `to_string`

**Type**: std::string

---

### `operator<<`

**Type**: std::ostream &

---

### `hosted_at`

**Type**: std::string

---

### `hosted_at`

**Type**: void

---

### `encode_uri_component`

**Type**: std::string

---

### `encode_uri`

**Type**: std::string

---

### `decode_uri_component`

**Type**: std::string

---

### `decode_uri`

**Type**: std::string

---

### `encode_path_component`

**Type**: std::string

---

### `decode_path_component`

**Type**: std::string

---

### `encode_query_component`

**Type**: std::string

---

### `decode_query_component`

**Type**: std::string

---

### `append_query_params`

**Type**: std::string

---

### `make_range_header`

**Type**: std::pair< std::string, std::string >

---

### `make_basic_authentication_header`

**Type**: std::pair< std::string, std::string >

---

### `make_bearer_token_authentication_header`

**Type**: std::pair< std::string, std::string >

---

### `get_client_ip`

**Type**: std::string

---

