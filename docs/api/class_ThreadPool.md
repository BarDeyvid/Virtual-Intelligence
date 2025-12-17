# class `httplib::ThreadPool`

## Summary

| Members | Descriptions |
|---------|--------------|
| struct `friend `[`worker`](#) |  |
| std::vector< std::thread > `variable `[`threads_`](#) |  |
| std::list< std::function< void()> > `variable `[`jobs_`](#) |  |
| bool `variable `[`shutdown_`](#) |  |
| size_t `variable `[`max_queued_requests_`](#) |  |
| std::condition_variable `variable `[`cond_`](#) |  |
| std::mutex `variable `[`mutex_`](#) |  |
| `function `[`ThreadPool`](#) |  |
| `function `[`ThreadPool`](#) |  |
| `function `[`~ThreadPool`](#) |  |
| bool `function `[`enqueue`](#) |  |
| void `function `[`shutdown`](#) |  |

## Members

### `worker`

**Type**: struct

---

### `threads_`

**Type**: std::vector< std::thread >

---

### `jobs_`

**Type**: std::list< std::function< void()> >

---

### `shutdown_`

**Type**: bool

---

### `max_queued_requests_`

**Type**: size_t

---

### `cond_`

**Type**: std::condition_variable

---

### `mutex_`

**Type**: std::mutex

---

### `ThreadPool`

---

### `ThreadPool`

---

### `~ThreadPool`

---

### `enqueue`

**Type**: bool

---

### `shutdown`

**Type**: void

---

