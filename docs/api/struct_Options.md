# struct `VoicePipeline::Options`

## Summary

| Members | Descriptions |
|---------|--------------|
| int `variable `[`n_threads`](#) |  Number of threads for Whisper processing. |
| std::string `variable `[`language`](#) |  Language setting. |
| float `variable `[`vad_rms_threshold`](#) |  RMS threshold for detecting speech. |
| int `variable `[`vad_silence_ms`](#) |  Silence duration in ms to consider end of a speech segment. |
| int `variable `[`vad_min_duration_ms`](#) |  Minimum speech duration in ms to process. |

## Members

### `n_threads`

**Type**: int

 Number of threads for Whisper processing.

---

### `language`

**Type**: std::string

 Language setting.

---

### `vad_rms_threshold`

**Type**: float

 RMS threshold for detecting speech.

---

### `vad_silence_ms`

**Type**: int

 Silence duration in ms to consider end of a speech segment.

---

### `vad_min_duration_ms`

**Type**: int

 Minimum speech duration in ms to process.

---

