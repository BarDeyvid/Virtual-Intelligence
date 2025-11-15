# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10) 

# Define the project name
project(Virtual-Intelligence C CXX) 

# --- 1. Define Standard Compiler Settings ---
set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) 

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /Zi /W4") 
endif()

# --- 2. Define Source Files and Executable Target ---
set(MAIN_SOURCE ${CMAKE_SOURCE_DIR}/test_embedder.cpp) 
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/includes/Embedding/Embedder.cpp 
    ${CMAKE_SOURCE_DIR}/includes/voice/ElevenLabsTTS.cpp 
    ${CMAKE_SOURCE_DIR}/includes/voice/ElevenLabsTTS.cpp 
    ${CMAKE_SOURCE_DIR}/includes/voice/VoicePipeline.cpp 
    ${CMAKE_SOURCE_DIR}/includes/WeightedFusion/WeightedFusion.cpp 
    ${CMAKE_SOURCE_DIR}/includes/AlyssaMemoryHandler.cpp 
    ${CMAKE_SOURCE_DIR}/includes/AlyssaNet.cpp 
)

add_executable(Virtual_Intelligence_Target ${MAIN_SOURCE} ${COMMON_SOURCES}) 

# --- 3. Define Include Directories (Headers) ---

set(PROJECT_INCLUDES
    ${CMAKE_SOURCE_DIR}/includes 
    ${CMAKE_SOURCE_DIR}/includes/internal 
    ${CMAKE_SOURCE_DIR}/curl # For curl.h 
    #${CMAKE_SOURCE_DIR}/llama.cpp/common
)

set(EXTERNAL_IMPLEMENTATION
    ${CMAKE_SOURCE_DIR}/llama.cpp/build/bin/Release 
    ${CMAKE_SOURCE_DIR}/whisper.cpp/build/bin/Release 
)

# External Library Header Paths
set(EXTERNAL_INCLUDES
    ${CMAKE_SOURCE_DIR}/llama.cpp/include 
    ${CMAKE_SOURCE_DIR}/llama.cpp/ggml/include 
    ${CMAKE_SOURCE_DIR}/whisper.cpp/include 
    ${CMAKE_SOURCE_DIR}/whisper.cpp/ggml/include 

    # *** MISSING HEADER FIXES HERE ***
    C:/libs/portaudio/build/msvc 
    C:/libs/portaudio/include 
    C:/libs/ffmpeg/include 
    C:/libs/sqlite 
    C:/libs/curl/include 
    C:/libs/curl/bin #libcurl.lib is here 
)

# Apply all includes to the main target
target_include_directories(Virtual_Intelligence_Target PUBLIC
    ${PROJECT_INCLUDES} 
    ${EXTERNAL_INCLUDES} 
    ${EXTERNAL_IMPLEMENTATION} 
)

# --- 4. Define Library Directories and Link Libraries ---
target_link_directories(Virtual_Intelligence_Target PUBLIC
    
    C:/Users/Deyvid/Documents/Virtual-Intelligence/llama.cpp/build/bin 
    C:/Users/Deyvid/Documents/Virtual-Intelligence/whisper.cpp/build/src/Release 
    C:/Users/Deyvid/Documents/Virtual-Intelligence/whisper.cpp/build/ggml/src/Release 
    C:/Users/Deyvid/Documents/Virtual-Intelligence/llama.cpp/build/src/Release 
    C:/libs/portaudio/build/msvc 
    C:/libs/ffmpeg/lib 
    C:/libs/sqlite 
    C:/libs/curl/bin #libcurl.lib is here 
)

target_link_libraries(Virtual_Intelligence_Target PUBLIC
    portaudio.lib 
    avcodec.lib 
    avformat.lib 
    avutil.lib 
    swresample.lib 
    sqlite3.lib 
    
    # --- Libraries Added to Fix Linker Errors ---
    libcurl.lib     # <-- Fixes cURL errors 
    llama.lib       # <-- Fixes llama.cpp API errors 
    whisper.lib     # <-- Fixes whisper.cpp API errors 

    ggml.lib        # (Existing GGML dependency) 
)

# =================================================================================
# --- 5. AUTOMATIC DLL COPYING (POST-BUILD) ---
# =================================================================================

# Define the base directories (FFMPEG, CURL, SQLITE remain unchanged)
set(FFMPEG_BIN_DIR "C:/libs/ffmpeg/bin") 
set(CURL_BIN_DIR "C:/libs/curl/bin") 
set(SQLITE_BIN_DIR "C:/libs/sqlite") 

# FIX: Since you cannot rebuild in Debug, we explicitly point the paths 
# to the existing 'Release' DLLs for Llama/Whisper.

# Assumes PortAudio.dll is located here (as per your linker path C:/libs/portaudio/build/msvc)
set(PORTAUDIO_BIN_DIR "C:/libs/portaudio/build/msvc")
# Hardcode 'Release' for Llama/Whisper DLLs
set(LLAMA_BIN_DIR "${CMAKE_SOURCE_DIR}/llama.cpp/build/bin/Release")
set(WHISPER_BIN_DIR "${CMAKE_SOURCE_DIR}/whisper.cpp/build/bin/Release")

# 1. Define all fixed-path DLLs explicitly
set(FFMPEG_DLLS
    "${FFMPEG_BIN_DIR}/avcodec-62.dll"
    "${FFMPEG_BIN_DIR}/avdevice-62.dll"
    "${FFMPEG_BIN_DIR}/avfilter-11.dll"
    "${FFMPEG_BIN_DIR}/avformat-62.dll"
    "${FFMPEG_BIN_DIR}/avutil-60.dll"
    "${FFMPEG_BIN_DIR}/swresample-6.dll"
    "${FFMPEG_BIN_DIR}/swscale-9.dll"
)

set(CURL_DLLS
    "${CURL_BIN_DIR}/libcurl-x64.dll"
)

set(SQLITE_DLLS
    "${SQLITE_BIN_DIR}/sqlite3.dll"
)

# 2. Define configuration-dependent DLLs using the explicit 'Release' paths
set(CONFIG_DLLS
    # PortAudio DLL
    "${PORTAUDIO_BIN_DIR}/portaudio.dll" 

    # Llama/GGML DLLs (Now explicitly looking in 'Release')
    "${LLAMA_BIN_DIR}/llama.dll"
    "${LLAMA_BIN_DIR}/ggml.dll"
    "${LLAMA_BIN_DIR}/ggml-base.dll"
    "${LLAMA_BIN_DIR}/ggml-cpu.dll"
    "${LLAMA_BIN_DIR}/ggml-cuda.dll"
    
    # Whisper DLL (Now explicitly looking in 'Release')
    "${WHISPER_BIN_DIR}/whisper.dll"
)

# 3. Combine all lists into one variable for the copy command
list(APPEND RUNTIME_DLLS 
    ${FFMPEG_DLLS} 
    ${CURL_DLLS} 
    ${SQLITE_DLLS} 
    ${CONFIG_DLLS}
)

# 1. Command to ensure the /libs subdirectory exists
add_custom_command(
    TARGET Virtual_Intelligence_Target POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Virtual_Intelligence_Target>/libs" 
)

# 2. Command to copy all required runtime DLLs using the combined list
add_custom_command(
    TARGET Virtual_Intelligence_Target POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RUNTIME_DLLS}
        "$<TARGET_FILE_DIR:Virtual_Intelligence_Target>"
)