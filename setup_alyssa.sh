#!/bin/bash

# --- 1. Verificação de Privilégios ---
if [ "$EUID" -ne 0 ]; then
  echo "Solicitando sudo para dependências e permissões..."
  exec sudo "$0" "$@"
  exit
fi

# --- 2. Configurações de Ambiente e PATH ---
THREADS=$(nproc)
REAL_USER=$(logname)
AI_LIBS_ROOT="/home/$REAL_USER/Virtual-Intelligence"

# Garantir que o CMake encontre o NVCC no Arch Linux
export CUDA_PATH="/opt/cuda"
export PATH="$PATH:$CUDA_PATH/bin"

echo "Resolvendo dependências no Arch..."

# Resolver conflito do ONNX (Forçar versão CUDA para a 5060 Ti)
if pacman -Qi "onnxruntime-cpu" &> /dev/null; then
    echo "Removendo onnxruntime-cpu para instalar versão CUDA..."
    pacman -Rs --noconfirm onnxruntime-cpu
fi

DEPENDENCIES=(
    "base-devel" "cmake" "cuda" "opencv-cuda" "curl" "sqlite" 
    "ffmpeg" "portaudio" "protobuf" "pkgconf" "onnxruntime-cuda"
    "vtk" "hdf5" "glew" "double-conversion"
)

for pkg in "${DEPENDENCIES[@]}"; do
    if ! pacman -Qi "$pkg" &> /dev/null; then
        echo "Instalando $pkg..."
        pacman -S --needed --noconfirm "$pkg"
    fi
done

# --- 3. Garantir Repositórios ---
mkdir -p "$AI_LIBS_ROOT"

clone_repo() {
    local folder=$1
    local url=$2
    if [ ! -d "$AI_LIBS_ROOT/$folder" ]; then
        echo "Clonando $folder..."
        sudo -u "$REAL_USER" git clone "$url" "$AI_LIBS_ROOT/$folder"
    fi
}

clone_repo "llama.cpp" "https://github.com/ggerganov/llama.cpp.git"
clone_repo "whisper.cpp" "https://github.com/ggerganov/whisper.cpp.git"
clone_repo "spdlog" "https://github.com/gabime/spdlog.git"

# --- 4. Build dos Submódulos ---
build_submodule() {
    local folder=$1
    local name=$2
    echo "Compilando $name..."
    
    cd "$AI_LIBS_ROOT/$folder" || exit
    # Garantindo que o build use o PATH do CUDA exportado
    sudo -u "$REAL_USER" PATH="$PATH" cmake -B build \
        -DGGML_CUDA=ON \
        -DCMAKE_CUDA_ARCHITECTURES=native \
        -DCUDAToolkit_ROOT=/opt/cuda
    
    sudo -u "$REAL_USER" cmake --build build --config Release -j "$THREADS"
}

build_submodule "llama.cpp" "Llama.cpp"
build_submodule "whisper.cpp" "Whisper.cpp"

mkdir -p /home/deyvidb/Virtual-Intelligence/models
mkdir -p /home/deyvidb/Virtual-Intelligence/config

# --- 5. Download Automático de Modelos (AlyssaNet) ---
MODEL_DIR="$AI_LIBS_ROOT/models"
mkdir -p "$MODEL_DIR"

MODELS_TO_DOWNLOAD=(
    "gemma-3-1b-it-q4_0.gguf,https://huggingface.co/google/gemma-3-1b-it-qat-q4_0-gguf/resolve/main/gemma-3-1b-it-q4_0.gguf"
    "gemma-3-4b-it-q4_0.gguf,https://huggingface.co/google/gemma-3-4b-it-qat-q4_0-gguf/resolve/main/gemma-3-4b-it-q4_0.gguf"
    "embeddinggemma-300m-qat-Q4_0.gguf,https://huggingface.co/ggml-org/embeddinggemma-300M-GGUF/resolve/main/embeddinggemma-300M-Q8_0.gguf"
)

NEEDS_DOWNLOAD=false
for entry in "${MODELS_TO_DOWNLOAD[@]}"; do
    IFS="," read -r FILENAME URL <<< "$entry"
    if [ ! -f "$MODEL_DIR/$FILENAME" ]; then
        NEEDS_DOWNLOAD=true
        break
    fi
done

if [ "$NEEDS_DOWNLOAD" = true ]; then
    echo "--------------------------------------------------------"
    echo "Alguns modelos precisam de download (Hugging Face)."
    echo "Caso não tenha um token, crie em: https://huggingface.co/settings/tokens"
    # -p: prompt, -s: silent (esconde o que você digita)
    read -rs -p "Digite seu Token de Acesso: " HF_TOKEN
    echo -e "\n--------------------------------------------------------\n"
fi


echo "Verificando biblioteca de modelos..."

for entry in "${MODELS_TO_DOWNLOAD[@]}"; do
    IFS="," read -r FILENAME URL <<< "$entry"
    FILE_PATH="$MODEL_DIR/$FILENAME"

    if [ ! -f "$FILE_PATH" ]; then
        echo "Baixando: $FILENAME..."
        
        # -H "Authorization...": passa o token para o Hugging Face
        curl -C - -L \
            -H "Authorization: Bearer $HF_TOKEN" \
            "$URL" -o "$FILE_PATH"
        
        if [ $? -eq 0 ]; then
            echo "$FILENAME finalizado."
        else
            echo "Erro ao baixar $FILENAME."
        fi
    else
        echo "$FILENAME já está presente."
    fi
done
# --- 6. Build da AlyssaNet ---
echo "Compilando AlyssaNet..."
cd "$AI_LIBS_ROOT" || exit
sudo -u "$REAL_USER" PATH="$PATH" cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
sudo -u "$REAL_USER" cmake --build build -j "$THREADS"

echo "Tudo pronto! O NVCC foi localizado e a AlyssaNet está compilada."