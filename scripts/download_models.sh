#!/bin/bash

# Script to download Whisper models
# Usage: ./download_models.sh [model_name]
# Available models: tiny, base, small, medium, large-v1, large-v2, large-v3

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODELS_DIR="$SCRIPT_DIR/../models"
mkdir -p "$MODELS_DIR"

cd "$MODELS_DIR"

MODEL="${1:-base}"

case $MODEL in
    tiny|tiny.en)
        MODEL_FILE="ggml-tiny.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"
        ;;
    base|base.en)
        MODEL_FILE="ggml-base.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"
        ;;
    small|small.en)
        MODEL_FILE="ggml-small.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"
        ;;
    medium|medium.en)
        MODEL_FILE="ggml-medium.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin"
        ;;
    large|large-v1)
        MODEL_FILE="ggml-large-v1.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v1.bin"
        ;;
    large-v2)
        MODEL_FILE="ggml-large-v2.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v2.bin"
        ;;
    large-v3)
        MODEL_FILE="ggml-large-v3.bin"
        MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin"
        ;;
    all)
        echo "Downloading all models..."
        for m in tiny base small medium; do
            "$0" "$m"
        done
        exit 0
        ;;
    *)
        echo "Unknown model: $MODEL"
        echo "Usage: $0 [tiny|base|small|medium|large-v1|large-v2|large-v3|all]"
        exit 1
        ;;
esac

echo "Downloading Whisper model: $MODEL"
echo "URL: $MODEL_URL"
echo "Destination: $MODELS_DIR/$MODEL_FILE"

if command -v wget &> /dev/null; then
    wget -c "$MODEL_URL" -O "$MODEL_FILE"
elif command -v curl &> /dev/null; then
    curl -L -C - -o "$MODEL_FILE" "$MODEL_URL"
else
    echo "Error: wget or curl is required"
    exit 1
fi

echo ""
echo "Download complete: $MODEL_FILE"
echo "Size: $(du -h "$MODEL_FILE" | cut -f1)"
