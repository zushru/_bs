#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mode="${1:-targeted}"
actual_cpp="$(grep -R --include='*.cpp' -hE '^[[:space:]]*RUN_TEST\(' test | wc -l | tr -d ' ')"
actual_suites="$(find test -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"

if [[ "$actual_cpp" != "621" || "$actual_suites" != "35" ]]; then
  printf 'Inventory mismatch: C++ registrations=%s (expected 621), suites=%s (expected 35)\n' "$actual_cpp" "$actual_suites" >&2
  exit 1
fi

find_nm() {
  local candidate
  candidate="$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-nm"
  if [[ -x "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi
  candidate="$(find "$HOME/.platformio/packages/toolchain-xtensa-esp-elf" -type f \
    -name xtensa-esp32-elf-nm -perm -111 -print -quit 2>/dev/null || true)"
  if [[ -z "$candidate" ]]; then
    printf 'ERROR: xtensa-esp32-elf-nm not found\n' >&2
    return 1
  fi
  printf '%s\n' "$candidate"
}

verify_fileset_symbols() {
  local elf="$1"
  [[ -f "$elf" ]] || {
    printf 'ERROR: ELF not found: %s\n' "$elf" >&2
    return 2
  }
  local nm symbols method
  nm="$(find_nm)"
  symbols="$("$nm" -C --defined-only "$elf")"
  for method in countChecked monthKeyChecked fileSizeChecked readFile appendFile addFile truncateFile quarantineCopy quarantineFrom; do
    if ! grep -Fq "att::LittleFsFileSet::$method(" <<<"$symbols"; then
      printf 'FAIL: LittleFsFileSet::%s not retained in %s\n' "$method" "$elf" >&2
      return 1
    fi
    printf 'PASS: LittleFsFileSet::%s linked\n' "$method"
  done
}

case "$mode" in
  manifest)
    sha256sum -c MANIFEST.sha256
    ;;
  targeted)
    sha256sum -c MANIFEST.sha256
    bash -n tools/inspect_probe_wrappers.sh tools/verify_v0.2.71.sh
    test -x tools/inspect_probe_wrappers.sh
    printf 'v0.2.71 manifest, inventory and inspector tooling: PASS\n'
    ;;
  elf)
    shift
    if [[ "$#" -eq 0 ]]; then
      echo "Usage: $0 elf <esp32-adapter-link/firmware.elf> ..." >&2
      exit 2
    fi
    tools/inspect_probe_wrappers.sh "$@"
    for elf in "$@"; do
      verify_fileset_symbols "$elf"
    done
    ;;
  *)
    echo "Usage: $0 [manifest|targeted|elf <firmware.elf> ...]" >&2
    exit 2
    ;;
esac
