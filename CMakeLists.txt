cmake_minimum_required(VERSION 3.14)
project(VirtualIntelligence CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---

# 1. System Packages
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 2. SQLite3
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY sqlite3)

# 3. FFmpeg (avcodec, avformat, avutil, swresample)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# 4. PortAudio
find_library(PORTAUDIO_LIBRARY portaudio)

# --- AI Libraries (Paths based on your log) ---
set(AI_LIBS_ROOT "/home/deyvid/Virtual-Intelligence")

# Llama.cpp Setup
set(LLAMA_INCLUDE_DIR "${AI_LIBS_ROOT}/llama.cpp/include" "${AI_LIBS_ROOT}/llama.cpp/ggml/include")
set(LLAMA_LIB_DIR "${AI_LIBS_ROOT}/llama.cpp/build/bin")
link_directories(${LLAMA_LIB_DIR})

# Whisper.cpp Setup
set(WHISPER_INCLUDE_DIR "${AI_LIBS_ROOT}/whisper.cpp/include" "${AI_LIBS_ROOT}/whisper.cpp/ggml/include")
set(WHISPER_LIB_DIR "${AI_LIBS_ROOT}/whisper.cpp/build/src")
link_directories(${WHISPER_LIB_DIR})

# --- Include Directories ---
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${LLAMA_INCLUDE_DIR}
    ${WHISPER_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
)

# --- Source Files ---
set(SOURCES
    test_main.cpp
    AlyssaNet.cpp
    AlyssaMemoryHandler.cpp
    Embedding/Embedder.cpp
    WeightedFusion/WeightedFusion.cpp
    # Ensure these exist in the voice folder:
    voice/ElevenLabsTTS.cpp
    voice/VoicePipeline.cpp
)

# --- Executable ---
add_executable(Virtual-Intelligence-Test ${SOURCES})

# --- Linking ---
target_link_libraries(Virtual-Intelligence-Test PRIVATE
    llama
    ggml
    whisper
    ${CURL_LIBRARIES}
    ${SQLITE3_LIBRARY}
    ${PORTAUDIO_LIBRARY}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    Threads::Threads
    dl
    stdc++fs
)