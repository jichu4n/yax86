#!/bin/bash
# Download reference projects for yax86 emulator development

set -e

# Array of project URLs
declare -a PROJECTS=(
    "https://github.com/adriancable/8086tiny"
    "https://github.com/xrip/pico-xt"
    "https://github.com/mathijsvandenberg/picox86"
    "https://github.com/mikechambers84/XTulator"
    "https://github.com/jhhoward/Faux86"
    "https://github.com/640-KB/GLaBIOS"
)

# Project root directory
cd "$(dirname "$0")"/..

# Create .cache directory if it doesn't exist
mkdir -p .cache
cd .cache

# Clone each project
for url in "${PROJECTS[@]}"; do
    project_name=$(basename "${url}")
    if [ -d "${project_path}" ]; then
        echo "Project ${project_name} already exists, skipping..."
    else
        echo "Cloning ${project_name}..."
        git clone --depth 1 "${url}.git"
    fi
done

echo "All reference projects downloaded successfully!"
