cmake_minimum_required(VERSION 3.14)
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

# 3. FFmpeg (via find_library for Windows compatibility)
find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWRESAMPLE_LIBRARY swresample)

if(NOT AVCODEC_LIBRARY)
    message(WARNING "Could not find avcodec library. Audio features will be disabled.")
endif()

# 4. PortAudio
find_library(PORTAUDIO_LIBRARY portaudio)

# 5. ONNX Runtime
find_library(ONNXRUNTIME_LIBRARY onnxruntime)
if (NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "Could not find libonnxruntime.so.")
endif()

# 6. Protocol Buffers (using imported targets from protobuf-config.cmake)
if(NOT TARGET protobuf::libprotobuf)
    find_package(Protobuf REQUIRED)
endif()

# 7 FTXUI
find_package(ftxui REQUIRED)

# --- AI Libraries Root (Dinâmico e Portável) ---
set(AI_LIBS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Llama.cpp & Whisper.cpp Setup
set(LLAMA_INCLUDE_DIR 
    "${AI_LIBS_ROOT}/llama.cpp/include" 
    "${AI_LIBS_ROOT}/llama.cpp/ggml/include"
    "${AI_LIBS_ROOT}/llama.cpp/ggml/include/ggml"
)
# --- CUDA Build Paths (use build_cuda if CUDA is available, fallback to build) ---
if(DEFINED ENV{CUDA_PATH} OR EXISTS "${AI_LIBS_ROOT}/llama.cpp/build_cuda")
    set(LLAMA_BUILD_DIR "${AI_LIBS_ROOT}/llama.cpp/build_cuda")
    set(WHISPER_BUILD_DIR "${AI_LIBS_ROOT}/whisper.cpp/build_cuda")
    message(STATUS "Using CUDA-enabled build directories")
else()
    set(LLAMA_BUILD_DIR "${AI_LIBS_ROOT}/llama.cpp/build")
    set(WHISPER_BUILD_DIR "${AI_LIBS_ROOT}/whisper.cpp/build")
    message(STATUS "Using CPU-only build directories")
endif()

set(LLAMA_LIB_DIR "${LLAMA_BUILD_DIR}")

# Find actual library files (multi-config generator: Release/Debug subdirs)
# Order: prefer build_cuda > build to pick up CUDA-enabled libs
find_library(LLAMA_LIBRARY_FILE llama PATHS "${LLAMA_BUILD_DIR}/src" "${AI_LIBS_ROOT}/llama.cpp/build/src" PATH_SUFFIXES Release Debug NO_DEFAULT_PATH)
find_library(GGML_LIBRARY_FILE ggml PATHS "${LLAMA_BUILD_DIR}/ggml/src" "${AI_LIBS_ROOT}/llama.cpp/build/ggml/src" PATH_SUFFIXES Release Debug NO_DEFAULT_PATH)
find_library(GGML_BASE_LIBRARY_FILE ggml-base PATHS "${LLAMA_BUILD_DIR}/ggml/src" "${AI_LIBS_ROOT}/llama.cpp/build/ggml/src" PATH_SUFFIXES Release Debug NO_DEFAULT_PATH)
find_library(GGML_CPU_LIBRARY_FILE ggml-cpu PATHS "${LLAMA_BUILD_DIR}/ggml/src" "${AI_LIBS_ROOT}/llama.cpp/build/ggml/src" PATH_SUFFIXES Release Debug NO_DEFAULT_PATH)
find_library(WHISPER_LIBRARY_FILE whisper PATHS "${WHISPER_BUILD_DIR}/src" "${AI_LIBS_ROOT}/whisper.cpp/build/src" PATH_SUFFIXES Release Debug NO_DEFAULT_PATH)

message(STATUS "LLAMA lib: ${LLAMA_LIBRARY_FILE}")
message(STATUS "GGML lib: ${GGML_LIBRARY_FILE}")
message(STATUS "WHISPER lib: ${WHISPER_LIBRARY_FILE}")

set(WHISPER_INCLUDE_DIR 
    "${AI_LIBS_ROOT}/whisper.cpp/include" 
    "${AI_LIBS_ROOT}/whisper.cpp/ggml/include"
    "${AI_LIBS_ROOT}/whisper.cpp/ggml/include/ggml"
)

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
    ${OpenCV_INCLUDE_DIRS} 
)

# --- Common Source Files ---
set(COMMON_SOURCES includes/AlyssaNet.cpp includes/OpenCVExpert.hpp includes/pc_metrics_reader.cpp includes/EmotionLexiconLoader.cpp includes/AlyssaMemoryHandler.cpp includes/Embedding/Embedder.cpp includes/EndocrineSystem.cpp includes/WeightedFusion/WeightedFusion.cpp includes/voice/ElevenLabsTTS.cpp includes/voice/VoicePipeline.cpp includes/log.cpp)

# --- Source Files ---
set(Alyssa_CLI_SOURCES Alyssa_CLI.cpp ${COMMON_SOURCES})
set(Alyssa_CLI_WITH_VOICE_SOURCES Alyssa_CLI_WITH_VOICE.cpp ${COMMON_SOURCES})

# Tests Sources
set(TEST_TTS tests/test_elevenlabs.cpp includes/voice/ElevenLabsTTS.cpp)
set(TEST_VOICE tests/test_voice.cpp includes/voice/VoicePipeline.cpp)
set(TEST_OPENCV test_cuda.cpp)
set(TEST_FUSION_UTILS tests/test_fusion_utils.cpp)

# --- Executables ---
add_executable(alyssa_cli ${Alyssa_CLI_SOURCES})
add_executable(alyssa_cli_with_voice ${Alyssa_CLI_WITH_VOICE_SOURCES})
add_executable(test_elevenlabs ${TEST_TTS})
add_executable(test_voice ${TEST_VOICE})
add_executable(test_fusion_utils ${TEST_FUSION_UTILS})

# --- Linking ---
set(COMMON_LIBS
    ${LLAMA_LIBRARY_FILE}
    ${GGML_LIBRARY_FILE}
    ${WHISPER_LIBRARY_FILE}
    ${ONNXRUNTIME_LIBRARY}
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
    ${CURL_LIBRARIES}
    ${SQLITE3_LIBRARY}
    ${PORTAUDIO_LIBRARY}
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ${OpenCV_LIBS} 
    Threads::Threads
    spdlog::spdlog
)

# Platform-specific libraries
if(NOT WIN32)
    target_link_libraries(alyssa_cli PRIVATE ${COMMON_LIBS} ftxui::screen ftxui::dom ftxui::component dl stdc++fs)
    target_link_libraries(alyssa_cli_with_voice PRIVATE ${COMMON_LIBS} dl stdc++fs)
    target_link_libraries(test_voice PRIVATE ${WHISPER_LIBRARY_FILE} protobuf::libprotobuf protobuf::libprotobuf-lite ${PORTAUDIO_LIBRARY} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWRESAMPLE_LIBRARY} Threads::Threads dl stdc++fs spdlog::spdlog)
    target_link_libraries(test_elevenlabs PRIVATE protobuf::libprotobuf protobuf::libprotobuf-lite ${CURL_LIBRARIES} ${PORTAUDIO_LIBRARY} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWRESAMPLE_LIBRARY} Threads::Threads dl stdc++fs spdlog::spdlog)
else()
    target_link_libraries(alyssa_cli PRIVATE ${COMMON_LIBS} ftxui::screen ftxui::dom ftxui::component)
    target_link_libraries(alyssa_cli_with_voice PRIVATE ${COMMON_LIBS})
    target_link_libraries(test_voice PRIVATE ${WHISPER_LIBRARY_FILE} protobuf::libprotobuf protobuf::libprotobuf-lite ${PORTAUDIO_LIBRARY} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWRESAMPLE_LIBRARY} Threads::Threads spdlog::spdlog)
    target_link_libraries(test_elevenlabs PRIVATE protobuf::libprotobuf protobuf::libprotobuf-lite ${CURL_LIBRARIES} ${PORTAUDIO_LIBRARY} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWRESAMPLE_LIBRARY} Threads::Threads spdlog::spdlog)
endif()

# --- Post-Build Commands ---
foreach(TARGET alyssa_cli alyssa_cli_with_voice)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/models
            $<TARGET_FILE_DIR:${TARGET}>/models
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:${TARGET}>/config
        COMMENT "Copying models and config directories to build folder"
    )

    #  SOLUÇÃO DA GAMBIARRA PARA WINDOWS 
    # Copia as DLLs do llama.cpp, whisper.cpp e vcpkg direto para a pasta destino do executável
    if(WIN32)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            # 1. Copiar DLLs do llama.cpp (Se existirem no build_cuda ou build padrão)
            COMMAND ${CMAKE_COMMAND} -E echo "Copiando DLLs do llama.cpp para $<TARGET_FILE_DIR:${TARGET}>..."
            COMMAND ${CMAKE_COMMAND} -E copy_if_newer "${LLAMA_BUILD_DIR}/bin/$<CONFIG>/llama.dll" "$<TARGET_FILE_DIR:${TARGET}>/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_newer "${LLAMA_BUILD_DIR}/bin/$<CONFIG>/ggml.dll" "$<TARGET_FILE_DIR:${TARGET}>/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_newer "${LLAMA_BUILD_DIR}/bin/$<CONFIG>/ggml-base.dll" "$<TARGET_FILE_DIR:${TARGET}>/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_newer "${LLAMA_BUILD_DIR}/bin/$<CONFIG>/ggml-cpu.dll" "$<TARGET_FILE_DIR:${TARGET}>/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${LLAMA_BUILD_DIR}/bin/$<CONFIG>" "$<TARGET_FILE_DIR:${TARGET}>"

            # 2. Copiar DLLs do whisper.cpp
            COMMAND ${CMAKE_COMMAND} -E echo "Copiando DLLs do whisper.cpp para $<TARGET_FILE_DIR:${TARGET}>..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${WHISPER_BUILD_DIR}/bin/$<CONFIG>" "$<TARGET_FILE_DIR:${TARGET}>"

            # 3. Copiar TODAS as DLLs de dependências instaladas pelo vcpkg
            COMMAND ${CMAKE_COMMAND} -E echo "Copiando DLLs do vcpkg para $<TARGET_FILE_DIR:${TARGET}>..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin" "$<TARGET_FILE_DIR:${TARGET}>"
        )
    endif()
endforeach()