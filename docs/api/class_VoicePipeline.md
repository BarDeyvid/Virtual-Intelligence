# class `VoicePipeline`

 Class representing the voice processing pipeline.

## Detailed Description

 This class handles audio capture, Voice Activity Detection (VAD), and transcription using Whisper model.

## Summary

| Members | Descriptions |
|---------|--------------|
| `variable `[`m_options`](#) |  Configuration options. |
| struct whisper_context * `variable `[`m_ctx`](#) |  Whisper model context. |
| PaStream * `variable `[`m_stream`](#) |  PortAudio stream. |
| `variable `[`m_audio_data`](#) |  Internal audio buffer storage. |
| size_t `variable `[`m_vad_min_samples`](#) |  Minimum samples for VAD to consider speech. |
| const size_t `variable `[`m_buffer_size_samples`](#) |  |
| const int `variable `[`m_pa_frames_per_buffer`](#) |  |
| `variable `[`m_input_queue`](#) |  PortAudio frames per buffer. |
| `variable `[`m_output_queue`](#) |  Queue for transcription results. |
| std::atomic< bool > `variable `[`m_running`](#) |  Atomic flag indicating if the pipeline is running. |
| std::atomic< bool > `variable `[`m_is_paused`](#) |  Atomic flag indicating if processing is paused. |
| std::thread `variable `[`m_worker_thread`](#) |  Thread for Whisper processing. |
| std::thread `variable `[`m_vad_thread`](#) |  Thread for VAD and audio segmenting. |
| `function `[`VoicePipeline`](#) |  Constructor. |
| `function `[`~VoicePipeline`](#) |  Destructor. |
| bool `function `[`start`](#) |  Starts audio capture and processing threads. |
| void `function `[`stop`](#) |  Stops audio capture and processing threads. |
| bool `function `[`get_last_result`](#) |  Gets the last transcription result from the output queue (non-blocking). |
| void `function `[`pause`](#) |  Pauses audio processing (VAD and callback). |
| void `function `[`resume`](#) |  Resumes audio processing. |
| void `function `[`_whisper_worker_func`](#) |  Function for the Whisper worker thread. |
| void `function `[`_vad_loop_func`](#) |  Function for the VAD loop thread. |
| int `function `[`_pa_callback_impl`](#) |  PortAudio callback implementation. |
| std::string `function `[`_process_transcription`](#) |  Processes an audio buffer for transcription. |
| bool `function `[`_is_speech`](#) |  Simple Voice Activity Detection (VAD) logic. |
| int `function `[`_pa_callback`](#) |  Static PortAudio callback function. |

## Members

### `m_options`

 Configuration options.

---

### `m_ctx`

**Type**: struct whisper_context *

 Whisper model context.

---

### `m_stream`

**Type**: PaStream *

 PortAudio stream.

---

### `m_audio_data`

 Internal audio buffer storage.

---

### `m_vad_min_samples`

**Type**: size_t

 Minimum samples for VAD to consider speech.

---

### `m_buffer_size_samples`

**Type**: const size_t

---

### `m_pa_frames_per_buffer`

**Type**: const int

---

### `m_input_queue`

 PortAudio frames per buffer.

---

### `m_output_queue`

 Queue for transcription results.

---

### `m_running`

**Type**: std::atomic< bool >

 Atomic flag indicating if the pipeline is running.

---

### `m_is_paused`

**Type**: std::atomic< bool >

 Atomic flag indicating if processing is paused.

---

### `m_worker_thread`

**Type**: std::thread

 Thread for Whisper processing.

---

### `m_vad_thread`

**Type**: std::thread

 Thread for VAD and audio segmenting.

---

### `VoicePipeline`

 Constructor.

---

### `~VoicePipeline`

 Destructor.

---

### `start`

**Type**: bool

 Starts audio capture and processing threads.

---

### `stop`

**Type**: void

 Stops audio capture and processing threads.

---

### `get_last_result`

**Type**: bool

 Gets the last transcription result from the output queue (non-blocking).

---

### `pause`

**Type**: void

 Pauses audio processing (VAD and callback).

---

### `resume`

**Type**: void

 Resumes audio processing.

---

### `_whisper_worker_func`

**Type**: void

 Function for the Whisper worker thread.

---

### `_vad_loop_func`

**Type**: void

 Function for the VAD loop thread.

---

### `_pa_callback_impl`

**Type**: int

 PortAudio callback implementation.

---

### `_process_transcription`

**Type**: std::string

 Processes an audio buffer for transcription.

---

### `_is_speech`

**Type**: bool

 Simple Voice Activity Detection (VAD) logic.

---

### `_pa_callback`

**Type**: int

 Static PortAudio callback function.

---

