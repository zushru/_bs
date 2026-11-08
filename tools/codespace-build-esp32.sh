#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Compile/link only. This script deliberately has no upload/flash command.
printf '\n=== Inert adapter compile/link smoke target ===\n'
pio run -j 2 -c platformio.esp32-smoke.ini -e esp32-adapter-link

printf '\n=== Diagnostic hardware-probe compile/link target ===\n'
pio run -j 2 -c platformio.esp32-probe.ini -e esp32-hardware-probe

printf '\n=== Smoke ELF NVS-wrapper routing inspection ===\n'
bash tools/inspect_probe_wrappers.sh .pio/build/esp32-adapter-link/firmware.elf

printf '\n=== Probe ELF NVS-wrapper routing inspection ===\n'
bash tools/inspect_probe_wrappers.sh .pio/build/esp32-hardware-probe/firmware.elf

printf '\nESP32_TARGET_BUILDS=PASS (compile/link only; no hardware test)\n'
