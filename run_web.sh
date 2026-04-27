#!/bin/bash
set -e

emcc -o index.html main.c \
    /home/user/src/raylib/src/libraylib.a \
    -I. -I/home/user/src/raylib/src \
    -s USE_GLFW=3 \
    -s ASYNCIFY \
    -s USE_SQLITE3=1 \
    -s FORCE_FILESYSTEM=1 \
    -s EXPORTED_FUNCTIONS=['_main','_on_fs_loaded'] \
    -s EXPORTED_RUNTIME_METHODS=['ccall'] \
    -lidbfs.js \
    --preload-file data \
    -DPLATFORM_WEB \
    --shell-file ./shell.html \
    -lGL

python3 -m http.server 8000