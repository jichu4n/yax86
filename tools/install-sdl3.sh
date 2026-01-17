#!/bin/bash
set -e

# Install dependencies for SDL3
sudo apt-get update
sudo apt-get install -y \
    libasound2-dev \
    libpulse-dev \
    libaudio-dev \
    libjack-dev \
    libsndio-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxi-dev \
    libxss-dev \
    libxkbcommon-dev \
    libdbus-1-dev \
    libudev-dev \
    libwayland-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    libglu1-mesa-dev \
    libiberty-dev \
    libgbm-dev

# Use a temporary directory for building SDL3
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Clone SDL3
git clone https://github.com/libsdl-org/SDL.git -b release-3.2.0 --depth 1

# Build and install SDL3
mkdir -p SDL/build
cd SDL/build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
sudo cmake --install .
sudo ldconfig

# Cleanup
cd /
rm -rf "$TEMP_DIR"
