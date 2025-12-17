# struct `sqlite3_vfs`

## Summary

| Members | Descriptions |
|---------|--------------|
| int `variable `[`iVersion`](#) |  |
| int `variable `[`szOsFile`](#) |  |
| int `variable `[`mxPathname`](#) |  |
| `variable `[`pNext`](#) |  |
| const char * `variable `[`zName`](#) |  |
| void * `variable `[`pAppData`](#) |  |
| int(* `variable `[`xOpen`](#) |  |
| int(* `variable `[`xDelete`](#) |  |
| int(* `variable `[`xAccess`](#) |  |
| int(* `variable `[`xFullPathname`](#) |  |
| void *(* `variable `[`xDlOpen`](#) |  |
| void(* `variable `[`xDlError`](#) |  |
| void(*(* `variable `[`xDlSym`](#) |  |
| void(* `variable `[`xDlClose`](#) |  |
| int(* `variable `[`xRandomness`](#) |  |
| int(* `variable `[`xSleep`](#) |  |
| int(* `variable `[`xCurrentTime`](#) |  |
| int(* `variable `[`xGetLastError`](#) |  |
| int(* `variable `[`xCurrentTimeInt64`](#) |  |
| int(* `variable `[`xSetSystemCall`](#) |  |
| `variable `[`xGetSystemCall`](#) |  |
| const char *(* `variable `[`xNextSystemCall`](#) |  |

## Members

### `iVersion`

**Type**: int

---

### `szOsFile`

**Type**: int

---

### `mxPathname`

**Type**: int

---

### `pNext`

---

### `zName`

**Type**: const char *

---

### `pAppData`

**Type**: void *

---

### `xOpen`

**Type**: int(*

---

### `xDelete`

**Type**: int(*

---

### `xAccess`

**Type**: int(*

---

### `xFullPathname`

**Type**: int(*

---

### `xDlOpen`

**Type**: void *(*

---

### `xDlError`

**Type**: void(*

---

### `xDlSym`

**Type**: void(*(*

---

### `xDlClose`

**Type**: void(*

---

### `xRandomness`

**Type**: int(*

---

### `xSleep`

**Type**: int(*

---

### `xCurrentTime`

**Type**: int(*

---

### `xGetLastError`

**Type**: int(*

---

### `xCurrentTimeInt64`

**Type**: int(*

---

### `xSetSystemCall`

**Type**: int(*

---

### `xGetSystemCall`

---

### `xNextSystemCall`

**Type**: const char *(*

---

