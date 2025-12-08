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

# --- AI Libraries Root ---
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
    ${PROJECT_SOURCE_DIR}/includes
    ${LLAMA_INCLUDE_DIR}
    ${WHISPER_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
)

# --- Source Files for each test ---
set(TEST_MAIN_CLI_SOURCES
    test_main_cli.cpp
    includes/AlyssaNet.cpp
    includes/AlyssaMemoryHandler.cpp
    includes/Embedding/Embedder.cpp
    includes/WeightedFusion/WeightedFusion.cpp
    includes/voice/ElevenLabsTTS.cpp
    includes/voice/VoicePipeline.cpp
    includes/log.cpp
)

set(TEST_MAIN_WHISPER_SOURCES
    test_main_whisper.cpp
    includes/AlyssaNet.cpp
    includes/AlyssaMemoryHandler.cpp
    includes/Embedding/Embedder.cpp
    includes/WeightedFusion/WeightedFusion.cpp
    includes/voice/ElevenLabsTTS.cpp
    includes/voice/VoicePipeline.cpp
    includes/log.cpp
)

set(TEST_API_SOURCES
    test_api.cpp
    includes/AlyssaNet.cpp
    includes/AlyssaMemoryHandler.cpp
    includes/Embedding/Embedder.cpp
    includes/WeightedFusion/WeightedFusion.cpp
    includes/voice/ElevenLabsTTS.cpp
    includes/voice/VoicePipeline.cpp
    includes/log.cpp
)

# --- Executables ---
add_executable(test_main_cli ${TEST_MAIN_CLI_SOURCES})
add_executable(test_main_whisper ${TEST_MAIN_WHISPER_SOURCES})
add_executable(test_api ${TEST_API_SOURCES})

# --- Linking ---
target_link_libraries(test_main_cli PRIVATE
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

target_link_libraries(test_main_whisper PRIVATE
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

target_link_libraries(test_api PRIVATE
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

# --- Post-Build Commands (Copy assets) ---
# This copies 'models' and 'config' to the build folder automatically
foreach(TARGET test_main_cli test_main_whisper test_api)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/models
            ${CMAKE_BINARY_DIR}/models
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/config
            ${CMAKE_BINARY_DIR}/config
        COMMENT "Copying models and config directories to build folder"
    )
endforeach()
