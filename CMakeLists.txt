# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10) 

# Define the project name
project(Virtual-Intelligence C CXX) 

# ------------------------------------------------
# --- 1. CONFIGURAÇÕES COMUNS ---
# ------------------------------------------------
set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) 

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /Zi /W4") 
endif()

# ------------------------------------------------
# --- 2. DEFINIÇÃO DE FONTES COMUNS E BIBLIOTECA ---
# ------------------------------------------------

# Fontes (Sources) que são usadas por todos os executáveis
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/includes/Embedding/Embedder.cpp 
    ${CMAKE_SOURCE_DIR}/includes/voice/ElevenLabsTTS.cpp 
    ${CMAKE_SOURCE_DIR}/includes/voice/VoicePipeline.cpp 
    ${CMAKE_SOURCE_DIR}/includes/WeightedFusion/WeightedFusion.cpp 
    ${CMAKE_SOURCE_DIR}/includes/AlyssaMemoryHandler.cpp 
    ${CMAKE_SOURCE_DIR}/includes/AlyssaNet.cpp 
    # Duas ocorrências de ElevenLabsTTS.cpp foram removidas. Se for intencional, adicione de volta.
)

# CRIA UMA BIBLIOTECA ESTÁTICA COM TODOS OS ARQUIVOS COMUNS
add_library(Virtual_Intelligence_Common STATIC ${COMMON_SOURCES})

# ------------------------------------------------
# --- 3. DEFINIÇÃO DE INCLUDES E LIBRARIES (COMPARTILHADO) ---
# ------------------------------------------------

# Diretórios de Inclusão do Projeto (Headers)
set(PROJECT_INCLUDES
    ${CMAKE_SOURCE_DIR}/includes 
    ${CMAKE_SOURCE_DIR}/includes/internal 
    ${CMAKE_SOURCE_DIR}/curl # For curl.h 
)

set(EXTERNAL_IMPLEMENTATION
    ${CMAKE_SOURCE_DIR}/llama.cpp/build/bin/Release 
    ${CMAKE_SOURCE_DIR}/whisper.cpp/build/bin/Release 
)

# Diretórios de Inclusão de Bibliotecas Externas (Headers)
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

# Aplica todos os includes APENAS à biblioteca comum
target_include_directories(Virtual_Intelligence_Common PUBLIC
    ${PROJECT_INCLUDES} 
    ${EXTERNAL_INCLUDES} 
    ${EXTERNAL_IMPLEMENTATION} 
)

# Diretórios de Bibliotecas para Linkagem
set(LINK_DIRECTORIES
    C:/Users/Deyvid/Documents/Virtual-Intelligence/llama.cpp/build/bin 
    C:/Users/Deyvid/Documents/Virtual-Intelligence/whisper.cpp/build/src/Release 
    C:/Users/Deyvid/Documents/Virtual-Intelligence/whisper.cpp/build/ggml/src/Release 
    C:/Users/Deyvid/Documents/Virtual-Intelligence/llama.cpp/build/src/Release 
    C:/libs/portaudio/build/msvc 
    C:/libs/ffmpeg/lib 
    C:/libs/sqlite 
    C:/libs/curl/bin #libcurl.lib is here 
)

# Bibliotecas para Linkagem
set(LINK_LIBRARIES
    portaudio.lib 
    avcodec.lib 
    avformat.lib 
    avutil.lib 
    swresample.lib 
    sqlite3.lib 
    libcurl.lib 
    llama.lib 
    whisper.lib 
    ggml.lib 
)

# Aplica os diretórios de linkagem APENAS à biblioteca comum
target_link_directories(Virtual_Intelligence_Common PUBLIC ${LINK_DIRECTORIES})

# Aplica a linkagem das bibliotecas APENAS à biblioteca comum
# O PRIVATE aqui é importante para que as dependências não sejam "propagadas" desnecessariamente.
target_link_libraries(Virtual_Intelligence_Common PRIVATE ${LINK_LIBRARIES})


# ------------------------------------------------
# --- 4. DEFINIÇÃO DOS EXECUTÁVEIS INDIVIDUAIS ---
# ------------------------------------------------

# 1. Executável test_api
add_executable(test_api ${CMAKE_SOURCE_DIR}/test_api.cpp)
# Linka à biblioteca comum, que já carrega TODAS as dependências.
target_link_libraries(test_api PUBLIC Virtual_Intelligence_Common)

# 2. Executável test_main_cli
add_executable(test_main_cli ${CMAKE_SOURCE_DIR}/test_main_cli.cpp)
target_link_libraries(test_main_cli PUBLIC Virtual_Intelligence_Common)

# 3. Executável test_main_whisper
add_executable(test_main_whisper ${CMAKE_SOURCE_DIR}/test_main_whisper.cpp)
target_link_libraries(test_main_whisper PUBLIC Virtual_Intelligence_Common)


# ------------------------------------------------
# --- 5. AUTOMATIC DLL COPYING (POST-BUILD) ---
# ------------------------------------------------

# Define a lista de todos os TARGETS que precisam do passo POST_BUILD
set(ALL_TARGETS 
    test_api 
    test_main_cli 
    test_main_whisper
)

# ... (Seções de definição de DLLs FFMPEG_DLLS, CURL_DLLS, etc., permanecem as mesmas)

# 1. Define all fixed-path DLLs explicitly
set(FFMPEG_BIN_DIR "C:/libs/ffmpeg/bin") 
set(CURL_BIN_DIR "C:/libs/curl/bin") 
set(SQLITE_BIN_DIR "C:/libs/sqlite") 
set(PORTAUDIO_BIN_DIR "C:/libs/portaudio/build/msvc")
set(LLAMA_BIN_DIR "${CMAKE_SOURCE_DIR}/llama.cpp/build/bin/Release")
set(WHISPER_BIN_DIR "${CMAKE_SOURCE_DIR}/whisper.cpp/build/bin/Release")

set(FFMPEG_DLLS
    "${FFMPEG_BIN_DIR}/avcodec-62.dll"
    "${FFMPEG_BIN_DIR}/avdevice-62.dll"
    # ... (Resto das DLLs FFMPEG)
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

list(APPEND RUNTIME_DLLS 
    ${FFMPEG_DLLS} 
    ${CURL_DLLS} 
    ${SQLITE_DLLS} 
    ${CONFIG_DLLS}
)

# Aplica os comandos de cópia POST_BUILD a CADA um dos seus novos executáveis
foreach(TARGET ${ALL_TARGETS})
    # 1. Comando para garantir que o subdiretório /libs exista
    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/libs" 
    )

    # 2. Comando para copiar todas as DLLs de tempo de execução necessárias
    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:${TARGET}>"
    )
endforeach()