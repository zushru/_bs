#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mode="${1:-targeted}"
actual_cpp="$(grep -R --include='*.cpp' -hE '^[[:space:]]*RUN_TEST\(' test | wc -l | tr -d ' ')"
actual_suites="$(find test -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"
actual_sf2="$(grep -RhoE '(^|[^[:alnum:]_])test[[:space:]]*[(]' web/sf2/test --include='*.test.mjs' | wc -l | tr -d ' ')"
actual_sf2_files="$(find web/sf2/test -maxdepth 1 -type f -name '*.test.mjs' | wc -l | tr -d ' ')"

if [[ "$actual_cpp" != "632" || "$actual_suites" != "35" ||
      "$actual_sf2" != "257" || "$actual_sf2_files" != "12" ]]; then
  printf 'Inventory mismatch: C++=%s/632 suites=%s/35 SF2=%s/257 files=%s/12\n' \
    "$actual_cpp" "$actual_suites" "$actual_sf2" "$actual_sf2_files" >&2
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

verify_config_backend_symbols() {
  local elf="$1"
  [[ -f "$elf" ]] || {
    printf 'ERROR: ELF not found: %s\n' "$elf" >&2
    return 2
  }
  local nm symbols
  nm="$(find_nm)"
  symbols="$("$nm" -C --defined-only "$elf")"
  for symbol in \
    'att::Esp32NvsConfigBackend::begin()' \
    'att::Esp32NvsStartupGuard::status()' \
    'att::Esp32NvsApCredentialStore::load(' \
    'att::Esp32NvsApCredentialStore::createIfAbsent(' \
    'att::Esp32NvsPinAttemptStore::loadFailureCount(' \
    'att::Esp32NvsPinAttemptStore::initializeFailureCount()' \
    'att::Esp32NvsPinAttemptStore::persistFailureCount('; do
    if ! grep -Fq "$symbol" <<<"$symbols"; then
      printf 'FAIL: %s not retained in %s\n' "$symbol" "$elf" >&2
      return 1
    fi
    printf 'PASS: %s linked\n' "$symbol"
  done
}

case "$mode" in
  manifest)
    sha256sum -c MANIFEST.sha256
    ;;
  targeted)
    sha256sum -c MANIFEST.sha256
    bash -n tools/inspect_probe_wrappers.sh tools/verify_v0.2.76.sh
    test -x tools/inspect_probe_wrappers.sh
    test -x tools/verify_v0.2.76.sh
    test -f RELEASE_v0.2.76.md
    test -f docs/AUDIT_v0.2.76.md
    printf 'v0.2.76 manifest, 632/35 C++ inventory, 257/12 SF2 inventory and tooling: PASS\n'
    ;;
  elf)
    shift
    if [[ "$#" -eq 0 ]]; then
      echo "Usage: $0 elf <esp32-adapter-link/firmware.elf> ..." >&2
      exit 2
    fi
    tools/inspect_probe_wrappers.sh "$@"
    for elf in "$@"; do
      verify_config_backend_symbols "$elf"
      verify_fileset_symbols "$elf"
    done
    ;;
  *)
    echo "Usage: $0 [manifest|targeted|elf <firmware.elf> ...]" >&2
    exit 2
    ;;
esac
