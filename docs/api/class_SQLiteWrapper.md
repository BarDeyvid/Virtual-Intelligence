# class `alyssa_memory::SQLiteWrapper`

 RAII wrapper for SQLite3 database connection.

## Detailed Description

 Manages SQLite database connection lifecycle with proper cleanup.

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`db_`](#) |  SQLite database handle. |
| `function `[`SQLiteWrapper`](#) |  Constructor with database path. |
| `function `[`~SQLiteWrapper`](#) |  Destructor - closes database connection. |
| `function `[`SQLiteWrapper`](#) |  |
| `function `[`operator=`](#) |  |
| `function `[`SQLiteWrapper`](#) |  Move constructor. |
| `function `[`get`](#) |  Get raw SQLite database pointer. |
| `function `[`operator sqlite3 *`](#) |  Conversion operator to sqlite3*. |

## Members

### `db_`

 SQLite database handle.

---

### `SQLiteWrapper`

 Constructor with database path.

---

### `~SQLiteWrapper`

 Destructor - closes database connection.

---

### `SQLiteWrapper`

---

### `operator=`

---

### `SQLiteWrapper`

 Move constructor.

---

### `get`

 Get raw SQLite database pointer.

---

### `operator sqlite3 *`

 Conversion operator to sqlite3*.

---

