#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${TMPDIR:-/tmp}/att-v0.2.51-verification"
MARKER="${TMPDIR:-/tmp}/att-v0.2.51-targeted.ok"
mkdir -p "$LOG_DIR"
cd "$ROOT"

mode="${1:-targeted}"
case "$mode" in
  targeted|links|full) ;;
  *) printf 'Usage: %s [targeted|links|full]\n' "$0" >&2; exit 2 ;;
esac

run_logged() {
  local label="$1"
  shift
  printf '\n===== %s =====\n' "$label"
  "$@" 2>&1 | tee "$LOG_DIR/$label.log"
}

tree_fingerprint() {
  find . -type f \
    ! -path './.git/*' \
    ! -path './.pio/*' \
    ! -path './node_modules/*' \
    ! -path '*/node_modules/*' \
    ! -path './.att_verify/*' \
    ! -name '*.tar.gz' ! -name '*.zip' \
    -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum | awk '{print $1}'
}

build_and_inspect() {
  local label="$1"
  local config="$2"
  local env="$3"
  local elf="$4"
  run_logged "$label-build" pio run -c "$config" -e "$env" -j 2
  # The two PlatformIO config files share .pio/build. Inspect each ELF before
  # building the next target, because the next invocation replaces that tree.
  run_logged "$label-elf-inspection" "$ROOT/tools/inspect_probe_wrappers.sh" "$ROOT/$elf"
}

printf 'Source root: %s\n' "$ROOT"
printf 'Logs: %s\n' "$LOG_DIR"
printf 'Mode: %s\n' "$mode"
printf 'This workflow never uploads or flashes.\n'

case "$mode" in
  targeted)
    rm -f "$MARKER"
    run_logged shell-syntax bash -n tools/inspect_probe_wrappers.sh tools/test_inspect_probe_wrappers.sh tools/verify_v0.2.51.sh
    run_logged inspector-fixtures bash tools/test_inspect_probe_wrappers.sh
    run_logged nvs-policy pio test -e native -f test_nvs_startup_guard_policy
    run_logged probe-credential pio test -e native -f test_hardware_probe
    run_logged as608 pio test -e native -f 'test_as608*'
    run_logged attendance-admission pio test -e native -f test_attendance_admission
    run_logged enrollment pio test -e native -f test_enroll
    tree_fingerprint > "$MARKER"
    printf '\nTARGETED_V0_2_51=PASS\n'
    printf 'Exact source fingerprint saved for links mode.\n'
    ;;
  links)
    if [[ ! -f "$MARKER" ]]; then
      printf 'ERROR: targeted verification marker missing; run targeted mode first.\n' >&2
      exit 3
    fi
    expected="$(cat "$MARKER")"
    actual="$(tree_fingerprint)"
    if [[ "$expected" != "$actual" ]]; then
      printf 'ERROR: source tree changed after targeted verification; rerun targeted mode.\n' >&2
      exit 4
    fi
    build_and_inspect esp32-smoke platformio.esp32-smoke.ini esp32-adapter-link .pio/build/esp32-adapter-link/firmware.elf
    build_and_inspect esp32-probe platformio.esp32-probe.ini esp32-hardware-probe .pio/build/esp32-hardware-probe/firmware.elf
    printf '\nESP32_LINK_AND_ELF_CHECKS=PASS\n'
    ;;
  full)
    rm -f "$MARKER"
    run_logged native-full pio test -e native
    run_logged sf2-node npm --prefix web/sf2 test
    run_logged sf2-syntax bash -c 'find web/sf2 -type f -name "*.mjs" -print0 | xargs -0 -r -n1 node --check'
    run_logged shell-syntax bash -n tools/inspect_probe_wrappers.sh tools/test_inspect_probe_wrappers.sh tools/verify_v0.2.51.sh
    run_logged inspector-fixtures bash tools/test_inspect_probe_wrappers.sh
    build_and_inspect esp32-smoke platformio.esp32-smoke.ini esp32-adapter-link .pio/build/esp32-adapter-link/firmware.elf
    build_and_inspect esp32-probe platformio.esp32-probe.ini esp32-hardware-probe .pio/build/esp32-hardware-probe/firmware.elf
    printf '\nFULL_V0_2_51_HOST_AND_LINK_CHECKS=PASS\n'
    ;;
esac

printf 'Review logs under: %s\n' "$LOG_DIR"
