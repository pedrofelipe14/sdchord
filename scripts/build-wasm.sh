#!/usr/bin/env bash
# Compiles the DSP core to web/public/sdchord.wasm. Needs Emscripten's em++ on PATH (or EMXX set).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p web/public

"${EMXX:-em++}" -std=c++17 -O3 -flto -fno-exceptions -fno-rtti \
    -Idsp/include dsp/src/wasm_api.cpp \
    -o web/public/sdchord.wasm \
    --no-entry -sSTANDALONE_WASM=1 \
    -sINITIAL_MEMORY=32MB -sALLOW_MEMORY_GROWTH=0 -sSTACK_SIZE=256KB

echo "web/public/sdchord.wasm: $(wc -c < web/public/sdchord.wasm) bytes"
