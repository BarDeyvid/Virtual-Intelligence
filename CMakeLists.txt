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

# 5. ONNX Runtime
find_library(ONNXRUNTIME_LIBRARY onnxruntime)
if (NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "Could not find libonnxruntime.so. Ensure the ONNX Runtime development package is installed.")
endif()

# 6. Protocol Buffers (Protobuf)
find_library(PROTOBUF_LIBRARY protobuf)
find_library(PROTOBUF_LITE_LIBRARY protobuf-lite)

if (NOT PROTOBUF_LIBRARY AND NOT PROTOBUF_LITE_LIBRARY)
    message(FATAL_ERROR "Could not find Protobuf libraries (libprotobuf or libprotobuf-lite). Ensure 'libprotobuf-dev' is installed.")
endif()

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
    ${PROJECT_SOURCE_DIR}/tests
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
    includes/ExpertBase.hpp
    includes/IExpert.hpp
    includes/EmotionLexiconLoader.cpp
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
    includes/EmotionLexiconLoader.cpp
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
    includes/EmotionLexiconLoader.cpp
    includes/AlyssaMemoryHandler.cpp
    includes/Embedding/Embedder.cpp
    includes/WeightedFusion/WeightedFusion.cpp
    includes/pc_metrics_reader.cpp
    includes/voice/ElevenLabsTTS.cpp
    includes/voice/VoicePipeline.cpp
    includes/log.cpp
)

set(TEST_TTS
    tests/test_elevenlabs.cpp
    includes/voice/ElevenLabsTTS.cpp
)

set(TEST_VOICE
    tests/test_voice.cpp
    includes/voice/VoicePipeline.cpp
)

# --- Executables ---
add_executable(test_main_cli ${TEST_MAIN_CLI_SOURCES})
add_executable(test_main_whisper ${TEST_MAIN_WHISPER_SOURCES})
add_executable(test_api ${TEST_API_SOURCES})
add_executable(test_elevenlabs ${TEST_TTS})
add_executable(test_voice ${TEST_VOICE})

# --- Linking ---
target_link_libraries(test_main_cli PRIVATE
    llama
    ggml
    whisper
    ${ONNXRUNTIME_LIBRARY}
    ${PROTOBUF_LIBRARY}      
    ${PROTOBUF_LITE_LIBRARY}   
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

target_link_libraries(test_elevenlabs PRIVATE
    ${PROTOBUF_LIBRARY}      
    ${PROTOBUF_LITE_LIBRARY}  
    ${CURL_LIBRARIES}
    ${PORTAUDIO_LIBRARY}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    Threads::Threads
    dl
    stdc++fs
)

target_link_libraries(test_voice PRIVATE
    whisper
    ${PROTOBUF_LIBRARY}      
    ${PROTOBUF_LITE_LIBRARY}  
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
    ${ONNXRUNTIME_LIBRARY}
    ${PROTOBUF_LIBRARY}      
    ${PROTOBUF_LITE_LIBRARY}  
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
    ${ONNXRUNTIME_LIBRARY}
    ${PROTOBUF_LIBRARY}      
    ${PROTOBUF_LITE_LIBRARY}  
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
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/models
            ${CMAKE_BINARY_DIR}/models
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/config
            ${CMAKE_BINARY_DIR}/config
        COMMAND ${CMAKE_COMMAND} -E copy_if_newer
            ${CMAKE_SOURCE_DIR}/fusion_router.onnx
            ${CMAKE_BINARY_DIR}/fusion_router.onnx
        COMMAND ${CMAKE_COMMAND} -E copy_if_newer
            ${CMAKE_SOURCE_DIR}/fusion_router.onnx.data
            ${CMAKE_BINARY_DIR}/fusion_router.onnx.data
        COMMENT "Copying models and config directories to build folder"
    )
endforeach()
