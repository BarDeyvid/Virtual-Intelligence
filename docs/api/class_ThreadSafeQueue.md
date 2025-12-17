# class `VoicePipeline::ThreadSafeQueue`

 Thread-safe queue for communication between threads.

## Detailed Description

 Template class to handle thread-safe operations on a queue.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::queue< T > `variable `[`q`](#) |  Internal queue storage. |
| std::mutex `variable `[`mtx`](#) |  Mutex for thread-safe access. |
| std::condition_variable `variable `[`cv`](#) |  Condition variable for synchronization. |
| bool `variable `[`running`](#) |  Flag indicating if the queue is still running. |
| void `function `[`push`](#) |  Pushes an item into the queue and notifies waiting consumers. |
| bool `function `[`pop`](#) |  Pops an item from the queue (blocking). |
| bool `function `[`try_pop`](#) |  Pops an item from the queue (non-blocking). |
| void `function `[`stop`](#) |  Stops all operations and notifies waiting consumers to exit. |

## Members

### `q`

**Type**: std::queue< T >

 Internal queue storage.

---

### `mtx`

**Type**: std::mutex

 Mutex for thread-safe access.

---

### `cv`

**Type**: std::condition_variable

 Condition variable for synchronization.

---

### `running`

**Type**: bool

 Flag indicating if the queue is still running.

---

### `push`

**Type**: void

 Pushes an item into the queue and notifies waiting consumers.

---

### `pop`

**Type**: bool

 Pops an item from the queue (blocking).

---

### `try_pop`

**Type**: bool

 Pops an item from the queue (non-blocking).

---

### `stop`

**Type**: void

 Stops all operations and notifies waiting consumers to exit.

---

