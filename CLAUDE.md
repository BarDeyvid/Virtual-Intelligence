# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**AlyssaNet (Virtual-Intelligence)** is an experimental C++20 AI entity that simulates human-like behavior through:
- **EndocrineSystem**: Simulated hormonal system (cortisol, dopamine, oxytocin, serotonin, adrenaline) that influences emotional state
- **Vision Pipeline**: Screenshot-based visual perception via OpenCV (currently ~500ms latency)
- **Mixture of Experts (MoE)**: Multiple LLM experts (emotional, analytical, memory) with weighted fusion for decision-making
- **Voice & Audio**: Whisper.cpp for speech-to-text, ElevenLabs API for TTS, PortAudio for I/O

The project is a personal experimental "XGH (Extreme Go Horse)" methodology codebase—PoCs and gambiarras (creative workarounds) are expected.

**Important**: This is not production code. It requires significant resources (36GB+ RAM, RTX GPU) and is tuned for the author's hardware (Ryzen 7 5800X, RTX 5060 TI, 56GB DDR4).

## Build & Development Commands

### Prerequisites
- **Compiler**: GCC 14+ or MSVC (C++17 minimum, C++20 preferred)
- **Build System**: CMake 3.14+
- **Package Manager**: vcpkg (for Windows dependencies)
- **Key Libraries**: OpenCV, CURL, SQLite3, PortAudio, ONNX Runtime, Protobuf, FTXUI, llama.cpp, whisper.cpp

### Build from Source

```bash
# Configure build (CPU-only)
mkdir build && cd build
cmake ..

# Configure build (CUDA-enabled, if CUDA_PATH is set or build_cuda exists)
# CMake automatically detects and uses build_cuda over build

# Compile
cmake --build . --config Release

# Build specific target
cmake --build . --config Release --target alyssa_cli
```

### Executables

| Target | Purpose |
|--------|---------|
| `alyssa_cli` | Main CLI with FTXUI interactive UI and hormonal system visualization |
| `alyssa_cli_with_voice` | CLI variant with voice I/O (Whisper + TTS) |
| `test_elevenlabs` | Test ElevenLabs TTS integration |
| `test_voice` | Test Whisper voice-to-text pipeline |
| `test_fusion_utils` | Test Mixture of Experts weighting algorithms |

### Running

```bash
# Main CLI (after building in build/)
./alyssa_cli

# Run with voice support
./alyssa_cli_with_voice

# Run specific test
./test_fusion_utils
```

### Post-Build

CMake automatically copies:
- `models/` directory (LLM weights) to build output
- `config/` directory (configuration files) to build output
- On Windows: DLLs from llama.cpp, whisper.cpp, and vcpkg to executable directory

Place `.gguf` model files (e.g., `gemma-3-4b-it-q4_0.gguf`) in the `models/` folder before running.

## Architecture

### Core Components

#### 1. **CoreIntegration (AlyssaNet.hpp/cpp)**
- Central brain orchestrator
- Manages LLM inference via llama.cpp
- Coordinates Mixture of Experts with weighted fusion
- Tracks user context and emotional state
- Public methods:
  - `initialize(model_path)`: Load LLM and initialize system
  - `think_with_fusion_ttsless(input)`: Process input and generate response (no voice)
  - `think_with_fusion(input)`: Process input with voice output
  - `get_endocrine_system()`: Access hormone profile

#### 2. **Mixture of Experts (ExpertBase.hpp, WeightedFusion/)**
- **ExpertBase**: Abstract expert implementation (emotional, analytical, memory models)
- **WeightedFusion**: Combines expert signals into final response
  - Each expert produces structured output: `[SINAL] value [CONFIANÇA] confidence [CONTEXTO] context`
  - Graceful degradation: strict format → generic patterns → raw response
  - Weights experts based on relevance to input context
- Expert types:
  - **emotionalModel**: Interprets emotional context
  - **analyticalModel**: Logical analysis
  - **memoryModel**: Fact retrieval and historical context

#### 3. **EndocrineSystem (EndocrineSystem.hpp/cpp)**
- Simulates human hormonal responses
- Hormones: cortisol, dopamine, oxytocin, serotonin, adrenaline
- Influences response generation and emotional state classification
- Outputs `HormoneProfile` with current emotional state (neutral, happy, angry, etc.)

#### 4. **Vision Pipeline (OpenCVExpert.hpp/cpp, VisionEnhancer.hpp/cpp)**
- Captures screenshots (via `grim` on Linux, Windows API on Windows)
- OpenCV processing: detection, enhancement, spatial reasoning
- ~500ms latency per frame (known bottleneck)

#### 5. **Voice I/O**
- **Whisper.cpp**: Speech-to-text, runs locally
- **ElevenLabs API**: Text-to-speech (requires API key in config)
- **PortAudio**: Audio device I/O

#### 6. **Memory & State (AlyssaMemoryHandler.hpp/cpp)**
- SQLite3 backend for persistent memory
- Stores conversation history, facts, emotional events
- Supports search and context retrieval

#### 7. **UI (Alyssa_CLI.cpp using FTXUI)**
- Real-time hormone gauge visualization
- Chat history with multi-tab interface (Chat, Logs, Metrics)
- Thread-safe state management with mutexes (`mtx` for UI state, `brain_mtx` for inference serialization)
- Custom logging callback routes llama.cpp output to UI

### Data Flow

```
User Input
    ↓
[Voice-to-Text via Whisper.cpp] (optional)
    ↓
EndocrineSystem adjusts hormones based on input emotional content
    ↓
Mixture of Experts processes input:
  - emotionalModel → emotional signal
  - analyticalModel → analytical signal
  - memoryModel → historical context
    ↓
WeightedFusion combines signals with confidence weights
    ↓
LLM (llama.cpp) generates response using fused signal + context
    ↓
Response → [Text-to-Speech via ElevenLabs] (optional)
    ↓
UI updates with response + hormone levels
```

## Development Notes

### Thread Safety
- **Critical**: `CoreIntegration::think_with_fusion*()` is **not thread-safe**. Wrap all calls in `brain_mtx` lock (see Alyssa_CLI.cpp lines 87-88).
- UI state (`chat_history`, `system_logs`, `hormones`) uses `mtx` lock.
- Input capture may miss events under high concurrency.

### Compiler Warnings
- MSVC warning 4996 is disabled for POSIX functions (`strdup`). Full migration to `std::string` is planned in Phase 4.
- See `AlyssaCore.hpp` lines 15-17 for suppression.

### Known Issues
- **Vision Pipeline**: 500ms latency per frame (bottleneck for real-time response)
- **Input Capture**: Sometimes misses keypresses under concurrent load
- **Endocrine Instability**: Hormone levels can fluctuate too rapidly; feedback control tuning needed
- **Windows Compatibility**: DLL copying in CMake is a workaround; `vcpkg` paths may need adjustment for different Windows configurations

### Refactoring Phases
Phase 4 includes migration from C-style string handling to `std::string` for safety. Phase 3 (current) focuses on core stability and MoE tuning.

### LLM Model Configuration

Model settings are defined in `SimpleModelConfig` (AlyssaCore.hpp):
- `n_ctx`: Context window (default 8192)
- `n_batch`: Batch size for token processing (default 8192)
- `temperature`: Randomness (0.8 default)
- `top_p`: Nucleus sampling (1.0 default)
- `max_tokens`: Generation limit (512 default)

Load multiple expert models via `AllModelConfigs = std::vector<SimpleModelConfig>`.

### Building on Different Platforms

- **Linux (Arch/Hyprland)**: Use FTXUI for UI, install `grim` for screenshots, CUDA support optional
- **Windows**: CMake detects CUDA via `%CUDA_PATH%` or `llama.cpp/build_cuda` presence; vcpkg manages dependencies
- **macOS**: Not tested; expect issues with input capture and platform-specific APIs

### Config Files

Place in `config/` directory (auto-copied to build output):
- API keys for external services (ElevenLabs, etc.)
- System prompts and role instructions
- Model paths and hyperparameters

## Key Files & Responsibilities

| File | Purpose |
|------|---------|
| `AlyssaNet.hpp/cpp` | CoreIntegration orchestrator and LLM inference |
| `ExpertBase.hpp` | Base expert with signal parsing and inference |
| `WeightedFusion/WeightedFusion.hpp/cpp` | Expert fusion algorithm |
| `EndocrineSystem.hpp/cpp` | Hormone simulation |
| `AlyssaMemoryHandler.hpp/cpp` | SQLite3 memory persistence |
| `Alyssa_CLI.cpp` | FTXUI UI and main event loop |
| `Alyssa_CLI_WITH_VOICE.cpp` | Voice-enabled variant |
| `includes/pc_metrics_reader.hpp/cpp` | System metrics (CPU, memory, GPU) |

## External Dependencies

- **llama.cpp**: Local submodule for LLM inference (CPU + CUDA support)
- **whisper.cpp**: Local submodule for speech-to-text
- **OpenCV**: System package (find_package)
- **CURL**: For HTTP requests to external APIs
- **SQLite3**: Local header + system library
- **ONNX Runtime**: External library for model inference
- **Protobuf**: Protocol buffer serialization
- **FTXUI**: Terminal UI framework (header-only style)
- **spdlog**: Logging library (local submodule)

## Debugging & Profiling

### Enable Verbose Logging
Set CMake flag or environment variable:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### Monitor Resource Usage
- `pc_metrics_reader.hpp/cpp` reads live CPU, memory, GPU metrics
- Logs go to `system_logs` vector in UI (Alyssa_CLI.cpp line 29)

### Trace Expert Signals
- Expert output is logged before fusion; check signal parsing in `ExpertBase::parse_expert_signal()` for format issues
- Graceful degradation means malformed signals fall back to raw response (logged but non-fatal)

## Common Tasks

### Add a New Expert
1. Inherit from `ExpertBase` or `IExpert`
2. Override `infer()` and optionally `parse_expert_signal()` for custom format
3. Add to expert array in `CoreIntegration::initialize()`
4. Register weights in `WeightedFusion` config

### Add a New Hormone
1. Add field to `HormoneProfile` struct (EndocrineSystem.hpp)
2. Implement update logic in `EndocrineSystem::update_endocrine_state()`
3. Add UI gauge in `RenderHormone()` (Alyssa_CLI.cpp line 52)

### Integrate External API
1. Use `httplib.h` (included in includes/) for HTTP
2. Parse response with `nlohmann/json.hpp` (included)
3. Store results in memory via `AlyssaMemoryHandler`
4. Add error handling (API timeouts expected)

### Change Model or Hyperparameters
1. Edit model path in `main()` or config file (Alyssa_CLI.cpp line 67)
2. Update `SimpleModelConfig` fields (temperature, context size, etc.)
3. Rebuild and test with small prompts first (inference is slow on large context)
