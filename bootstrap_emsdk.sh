#!/bin/bash
# Self-contained Emscripten bootstrap script
set -e

echo "=== Bootstrapping Emscripten SDK locally ==="

if [ ! -d "emsdk" ]; then
  echo "Cloning Emscripten SDK..."
  git clone --depth 1 https://github.com/emscripten-core/emsdk.git emsdk
else
  echo "emsdk directory already exists, skipping clone."
fi

cd emsdk
echo "Installing latest Emscripten version..."
./emsdk install latest
echo "Activating latest Emscripten version..."
./emsdk activate latest

echo "=== Emscripten SDK bootstrap complete! ==="
echo "To use: source emsdk/emsdk_env.sh"
