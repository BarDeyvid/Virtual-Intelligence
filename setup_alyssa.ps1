# --- 1. Verificação de Privilégios (Executar como Administrador) ---
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Solicitando permissões de Administrador..." -ForegroundColor Yellow
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    Exit
}

# --- 2. Configurações de Ambiente ---
$THREADS = $env:NUMBER_OF_PROCESSORS
$USER_PROFILE = $env:USERPROFILE
$AI_LIBS_ROOT = "$USER_PROFILE\Virtual-Intelligence"

Write-Host "Configurando ambiente com $THREADS threads..." -ForegroundColor Cyan

# INJETAR O CMAKE DO VISUAL STUDIO NO PATH
# O VS instala o CMake dentro da pasta do Common7. Vamos varrer os locais prováveis:
$VS_CMAKES = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
    "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
)

foreach ($path in $VS_CMAKES) {
    if (Test-Path $path) {
        $env:PATH = "$path;" + $env:PATH
        Write-Host "CMake localizado em: $path" -ForegroundColor Green
        break
    }
}

# --- 3. Instalação de Dependências via vcpkg (Otimizado) ---
$VCPKG_ROOT = "$AI_LIBS_ROOT\vcpkg"

if (-not (Test-Path $VCPKG_ROOT)) {
    Write-Host "Instalando vcpkg..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $AI_LIBS_ROOT | Out-Null
    git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT
    Start-Process -FilePath "$VCPKG_ROOT\bootstrap-vcpkg.bat" -Wait
}

# EVITAR COMPILAR EM DEBUG: Cria um arquivo de configuração para compilar APENAS Release (Corta tempo pela metade)
$TripletFile = "$VCPKG_ROOT\triplets\x64-windows.cmake"
if (Test-Path $TripletFile) {
    if (-not (Select-String -Path $TripletFile -Pattern "VCPKG_BUILD_TYPE")) {
        Add-Content -Path $TripletFile -Value "`nset(VCPKG_BUILD_TYPE release)"
        Write-Host "Configurado vcpkg para compilar APENAS em modo Release (Economiza tempo)." -ForegroundColor Yellow
    }
}

Write-Host "Instalando dependências pendentes via vcpkg..." -ForegroundColor Cyan
$env:CUDA_ARCH_BIN = "8.9" 
& "$VCPKG_ROOT\vcpkg" install curl sqlite3 ffmpeg portaudio protobuf onnxruntime ftxui --triplet x64-windows

# --- 4. Garantir Repositórios ---
function CloneRepo($folder, $url) {
    $target = "$AI_LIBS_ROOT\$folder"
    if (-not (Test-Path $target)) {
        Write-Host "Clonando $folder..." -ForegroundColor Green
        git clone $url $target
    }
}

CloneRepo "llama.cpp" "https://github.com/ggerganov/llama.cpp.git"
CloneRepo "whisper.cpp" "https://github.com/ggerganov/whisper.cpp.git"
CloneRepo "spdlog" "https://github.com/gabime/spdlog.git"

# --- 5. Build dos Submódulos ---
function BuildSubmodule($folder, $name) {
    Write-Host "Compilando $name..." -ForegroundColor Cyan
    Set-Location "$AI_LIBS_ROOT\$folder"
    
    if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
    
    # Executa usando o cmake injetado
    & cmake -B build `
        -A x64 `
        -DGGML_CUDA=ON `
        -DCMAKE_CUDA_ARCHITECTURES=89 `
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
        
    & cmake --build build --config Release -j $THREADS
}

BuildSubmodule "llama.cpp" "Llama.cpp"
BuildSubmodule "whisper.cpp" "Whisper.cpp"

New-Item -ItemType Directory -Force -Path "$AI_LIBS_ROOT\models" | Out-Null
New-Item -ItemType Directory -Force -Path "$AI_LIBS_ROOT\config" | Out-Null

# --- 6. Download Automático de Modelos (AlyssaNet) ---
#$MODEL_DIR = "$AI_LIBS_ROOT\models"
#$MODELS_TO_DOWNLOAD = @(
#    "gemma-3-1b-it-q4_0.gguf|https://huggingface.co/google/gemma-3-1b-it-qat-q4_0-gguf/resolve/main/gemma-3-1b-it-q4_0.gguf",
#    "gemma-3-4b-it-q4_0.gguf|https://huggingface.co/google/gemma-3-4b-it-qat-q4_0-gguf/resolve/main/gemma-3-4b-it-q4_0.gguf",
#    "embeddinggemma-300m-qat-Q4_0.gguf|https://huggingface.co/ggml-org/embeddinggemma-300M-GGUF/resolve/main/embeddinggemma-300M-Q8_0.gguf"
#)
#
#foreach ($model in $MODELS_TO_DOWNLOAD) {
#    $parts = $model -split "\|"
#    $filename = $parts[0]
#    $url = $parts[1]
#    $targetPath = "$MODEL_DIR\$filename"
#    
#    if (-not (Test-Path $targetPath)) {
#        Write-Host "Baixando modelo: $filename..." -ForegroundColor Green
#        Invoke-WebRequest -Uri $url -OutFile $targetPath
#    } else {
#        Write-Host "Modelo já existe: $filename" -ForegroundColor Yellow
#    }
#}

# --- 7. Build da AlyssaNet ---
Write-Host "Compilando AlyssaNet..." -ForegroundColor Cyan
Set-Location $AI_LIBS_ROOT

# Garante que nenhum lixo de configuração antiga trave o MSBuild
if (Test-Path "build") { 
    Write-Host "Limpando cache antigo de build da AlyssaNet..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build" 
}

# Configura o CMake apontando para a arquitetura nativa do seu VS (sem travar versão do VS antiga)
& cmake -B build `
    -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

& cmake --build build --config Release -j $THREADS