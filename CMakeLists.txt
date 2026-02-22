cmake_minimum_required(VERSION 3.14)
cmake_policy(SET CMP0146 OLD) # Silencia aviso de remoção do FindCUDA no OpenCV
project(VirtualIntelligence CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---

# 1. System Packages
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# OpenCV Setup
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV found: ${OpenCV_VERSION}")

# 2. SQLite3
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY sqlite3)

# 3. FFmpeg
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# 4. PortAudio
find_library(PORTAUDIO_LIBRARY portaudio)

# 5. ONNX Runtime
find_library(ONNXRUNTIME_LIBRARY onnxruntime)
if (NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "Could not find libonnxruntime.so.")
endif()

# 6. Protocol Buffers
find_library(PROTOBUF_LIBRARY protobuf)
find_library(PROTOBUF_LITE_LIBRARY protobuf-lite)

# --- AI Libraries Root ---
set(AI_LIBS_ROOT "/home/deyvid/Virtual-Intelligence")

# Llama.cpp & Whisper.cpp Setup
set(LLAMA_INCLUDE_DIR "${AI_LIBS_ROOT}/llama.cpp/include" "${AI_LIBS_ROOT}/llama.cpp/ggml/include")
set(LLAMA_LIB_DIR "${AI_LIBS_ROOT}/llama.cpp/build/bin")
link_directories(${LLAMA_LIB_DIR})

set(WHISPER_INCLUDE_DIR "${AI_LIBS_ROOT}/whisper.cpp/include" "${AI_LIBS_ROOT}/whisper.cpp/ggml/include")
set(WHISPER_LIB_DIR "${AI_LIBS_ROOT}/whisper.cpp/build/src")
link_directories(${WHISPER_LIB_DIR})

add_subdirectory(spdlog)
include_directories(${PROJECT_SOURCE_DIR}/spdlog/include)

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
    ${OpenCV_INCLUDE_DIRS} 
)

# --- Common Source Files ---
set(COMMON_SOURCES includes/AlyssaNet.cpp includes/OpenCVExpert.hpp includes/pc_metrics_reader.cpp includes/EmotionLexiconLoader.cpp includes/AlyssaMemoryHandler.cpp includes/Embedding/Embedder.cpp includes/EndocrineSystem.cpp includes/WeightedFusion/WeightedFusion.cpp includes/voice/ElevenLabsTTS.cpp includes/voice/VoicePipeline.cpp includes/log.cpp)

# --- Source Files ---
set(Alyssa_CLI_SOURCES Alyssa_CLI.cpp ${COMMON_SOURCES})
set(Alyssa_TRUE_SOURCES Alyssa_TRUE.cpp ${COMMON_SOURCES})
set(Alyssa_API_SOURCES Alyssa_API.cpp ${COMMON_SOURCES})

# Tests Sources
set(TEST_TTS tests/test_elevenlabs.cpp includes/voice/ElevenLabsTTS.cpp)
set(TEST_VOICE tests/test_voice.cpp includes/voice/VoicePipeline.cpp)
set(TEST_OPENCV test_cuda.cpp)

# --- Executables ---
add_executable(alyssa_cli ${Alyssa_CLI_SOURCES})
add_executable(alyssa_true ${Alyssa_TRUE_SOURCES})
add_executable(alyssa_api ${Alyssa_API_SOURCES})
add_executable(test_elevenlabs ${TEST_TTS})
add_executable(test_voice ${TEST_VOICE})
add_executable(test_opencv ${TEST_OPENCV})

# --- Linking ---
set(COMMON_LIBS
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
    ${OpenCV_LIBS} 
    Threads::Threads
    dl
    stdc++fs
    spdlog::spdlog
)

target_link_libraries(alyssa_cli PRIVATE ${COMMON_LIBS})
target_link_libraries(alyssa_true PRIVATE ${COMMON_LIBS})
target_link_libraries(alyssa_api PRIVATE ${COMMON_LIBS})

# Vinculando especificamente o teste do OpenCV
target_link_libraries(test_opencv PRIVATE ${OpenCV_LIBS} Threads::Threads) # ADICIONADO

target_link_libraries(test_elevenlabs PRIVATE ${PROTOBUF_LIBRARY} ${PROTOBUF_LITE_LIBRARY} ${CURL_LIBRARIES} ${PORTAUDIO_LIBRARY} ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWRESAMPLE_LIBRARIES} Threads::Threads dl stdc++fs spdlog::spdlog)
target_link_libraries(test_voice PRIVATE whisper ${PROTOBUF_LIBRARY} ${PROTOBUF_LITE_LIBRARY} ${PORTAUDIO_LIBRARY} ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWRESAMPLE_LIBRARIES} Threads::Threads dl stdc++fs spdlog::spdlog)

# --- Post-Build Commands ---
foreach(TARGET alyssa_cli alyssa_true alyssa_api)
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