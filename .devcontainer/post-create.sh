#!/usr/bin/env bash
set -euo pipefail

printf '\n=== _bs Codespaces environment ===\n'
python3 --version
node --version
npm --version
pio --version
mkdir -p "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
printf '\nSetup complete. No firmware is flashed automatically.\n'
