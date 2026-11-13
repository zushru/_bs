#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mode="${1:-targeted}"
actual_cpp="$(grep -R --include='*.cpp' -hE '^[[:space:]]*RUN_TEST\(' test | wc -l | tr -d ' ')"
actual_suites="$(find test -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"
actual_sf2="$(grep -RhoE '(^|[^[:alnum:]_])test[[:space:]]*[(]' web/sf2/test --include='*.test.mjs' | wc -l | tr -d ' ')"
actual_sf2_files="$(find web/sf2/test -maxdepth 1 -type f -name '*.test.mjs' | wc -l | tr -d ' ')"

if [[ "$actual_cpp" != "667" || "$actual_suites" != "37" ||
      "$actual_sf2" != "270" || "$actual_sf2_files" != "12" ]]; then
  printf 'Inventory mismatch: C++=%s/667 suites=%s/37 SF2=%s/270 files=%s/12\n' \
    "$actual_cpp" "$actual_suites" "$actual_sf2" "$actual_sf2_files" >&2
  exit 1
fi

find_nm() {
  local candidate
  candidate="$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-nm"
  if [[ -x "$candidate" ]]; then printf '%s\n' "$candidate"; return 0; fi
  candidate="$(find "$HOME/.platformio/packages/toolchain-xtensa-esp-elf" -type f -name xtensa-esp32-elf-nm -perm -111 -print -quit 2>/dev/null || true)"
  [[ -n "$candidate" ]] || { echo 'ERROR: xtensa-esp32-elf-nm not found' >&2; return 1; }
  printf '%s\n' "$candidate"
}

verify_symbols() {
  local elf="$1" nm symbols symbol
  [[ -f "$elf" ]] || { echo "ERROR: ELF not found: $elf" >&2; return 2; }
  nm="$(find_nm)"
  symbols="$("$nm" -C --defined-only "$elf")"
  for symbol in \
    'att::LittleFsStorageOwner::begin()' \
    'att::LittleFsStorageOwner::fileSet()' \
    'att::Esp32LittleFsMountBackend::mountWithoutFormatting(' \
    'att::Esp32LittleFsMountBackend::unmountOwned()' \
    'att::SerializedFileSet::countChecked(' \
    'att::SerializedFileSet::appendFile('; do
    if ! grep -Fq "$symbol" <<<"$symbols"; then
      printf 'FAIL: %s not retained in %s\n' "$symbol" "$elf" >&2
      return 1
    fi
    printf 'PASS: %s linked\n' "$symbol"
  done
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
    bash -n tools/inspect_probe_wrappers.sh tools/verify_v0.2.82.sh
    test -x tools/verify_v0.2.82.sh
    test -f RELEASE_v0.2.82.md
    test -f docs/AUDIT_v0.2.82.md
    printf 'v0.2.82 manifest, 667/37 C++ inventory, 270/12 SF2 inventory and tooling: PASS\n'
    ;;
  elf)
    shift
    [[ "$#" -gt 0 ]] || { echo "Usage: $0 elf <firmware.elf> ..." >&2; exit 2; }
    bash tools/inspect_probe_wrappers.sh "$@"
    for elf in "$@"; do verify_symbols "$elf"; done
    ;;
  *) echo "Usage: $0 [manifest|targeted|elf <firmware.elf> ...]" >&2; exit 2 ;;
esac
