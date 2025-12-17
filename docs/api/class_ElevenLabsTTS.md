# class `ElevenLabsTTS`

 Class for text-to-speech synthesis using ElevenLabs API.

## Summary

| Members | Descriptions |
|---------|--------------|
| std::string `variable `[`api_key_`](#) |  API key for ElevenLabs API. |
| std::string `variable `[`voice_id_`](#) |  Voice ID to use for TTS. |
| int `variable `[`sample_rate_`](#) |  Sample rate for the audio output. |
| PaStream * `variable `[`stream_`](#) |  PortAudio stream pointer. |
| bool `variable `[`ffmpeg_initialized_`](#) |  Flag indicating if FFmpeg is initialized. |
| const int `variable `[`CHANNELS`](#) |  Number of audio channels. |
| const PaSampleFormat `variable `[`PA_SAMPLE_TYPE`](#) |  Sample type for PortAudio. |
| const int `variable `[`FRAMES_PER_BUFFER`](#) |  Frames per buffer for PortAudio stream. |
| `function `[`ElevenLabsTTS`](#) |  Constructor for |
| `function `[`~ElevenLabsTTS`](#) |  Destructor for |
| void `function `[`synthesizeAndPlay`](#) |  Synthesizes and plays the given text using TTS. |
| void `function `[`initializePortAudio`](#) |  Initializes PortAudio. |
| void `function `[`terminatePortAudio`](#) |  Terminates PortAudio. |
| void `function `[`openAudioStream`](#) |  Opens the audio stream with PortAudio. |
| void `function `[`closeAudioStream`](#) |  Closes the audio stream with PortAudio. |
| void `function `[`initializeFFmpeg`](#) |  Initializes FFmpeg. |
| void `function `[`cleanupFFmpeg`](#) |  Cleans up FFmpeg resources. |
| std::string `function `[`cleanText`](#) |  Cleans the input text by removing unwanted characters and spaces. |
| std::vector< float > `function `[`generateAudio`](#) |  Generates audio from the given text using ElevenLabs API. |
| std::vector< float > `function `[`decodeAudioWithFFmpeg`](#) |  Decodes audio data with FFmpeg. |
| void `function `[`playAudio`](#) |  Plays the given audio data using PortAudio. |

## Members

### `api_key_`

**Type**: std::string

 API key for ElevenLabs API.

---

### `voice_id_`

**Type**: std::string

 Voice ID to use for TTS.

---

### `sample_rate_`

**Type**: int

 Sample rate for the audio output.

---

### `stream_`

**Type**: PaStream *

 PortAudio stream pointer.

---

### `ffmpeg_initialized_`

**Type**: bool

 Flag indicating if FFmpeg is initialized.

---

### `CHANNELS`

**Type**: const int

 Number of audio channels.

---

### `PA_SAMPLE_TYPE`

**Type**: const PaSampleFormat

 Sample type for PortAudio.

---

### `FRAMES_PER_BUFFER`

**Type**: const int

 Frames per buffer for PortAudio stream.

---

### `ElevenLabsTTS`

 Constructor for

---

### `~ElevenLabsTTS`

 Destructor for

---

### `synthesizeAndPlay`

**Type**: void

 Synthesizes and plays the given text using TTS.

---

### `initializePortAudio`

**Type**: void

 Initializes PortAudio.

---

### `terminatePortAudio`

**Type**: void

 Terminates PortAudio.

---

### `openAudioStream`

**Type**: void

 Opens the audio stream with PortAudio.

---

### `closeAudioStream`

**Type**: void

 Closes the audio stream with PortAudio.

---

### `initializeFFmpeg`

**Type**: void

 Initializes FFmpeg.

---

### `cleanupFFmpeg`

**Type**: void

 Cleans up FFmpeg resources.

---

### `cleanText`

**Type**: std::string

 Cleans the input text by removing unwanted characters and spaces.

---

### `generateAudio`

**Type**: std::vector< float >

 Generates audio from the given text using ElevenLabs API.

---

### `decodeAudioWithFFmpeg`

**Type**: std::vector< float >

 Decodes audio data with FFmpeg.

---

### `playAudio`

**Type**: void

 Plays the given audio data using PortAudio.

---

