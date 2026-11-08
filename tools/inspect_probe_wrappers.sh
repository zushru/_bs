#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ELF="${1:-$ROOT/.pio/build/esp32-hardware-probe/firmware.elf}"
TOOLCHAIN_DIR="${HOME}/.platformio/packages/toolchain-xtensa-esp-elf/bin"

if [[ ! -f "$ELF" ]]; then
  printf 'ERROR: ELF not found: %s\n' "$ELF" >&2
  exit 2
fi

find_tool() {
  local name="$1"
  if [[ -x "$TOOLCHAIN_DIR/$name" ]]; then
    printf '%s\n' "$TOOLCHAIN_DIR/$name"
    return 0
  fi
  local found
  found="$(find "${HOME}/.platformio/packages/toolchain-xtensa-esp-elf" \
    -type f -name "$name" -perm -111 -print -quit 2>/dev/null || true)"
  if [[ -z "$found" ]]; then
    printf 'ERROR: tool not found: %s\n' "$name" >&2
    return 1
  fi
  printf '%s\n' "$found"
}

NM="$(find_tool xtensa-esp32-elf-nm)"
OBJDUMP="$(find_tool xtensa-esp32-elf-objdump)"

require_match() {
  local description="$1"
  local pattern="$2"
  local input="$3"
  if ! grep -Eq "$pattern" <<<"$input"; then
    printf 'FAIL: %s\n' "$description" >&2
    return 1
  fi
  printf 'PASS: %s\n' "$description"
}

SYMBOLS="$("$NM" -C --defined-only "$ELF")"
CORE="$("$OBJDUMP" -dC --disassemble=initArduino "$ELF")"
WRAP_NVS="$("$OBJDUMP" -dC --disassemble=__wrap_nvs_flash_init "$ELF")"
WRAP_ERASE="$("$OBJDUMP" -dC --disassemble=__wrap_esp_partition_erase_range "$ELF")"

require_match 'NVS-init wrapper is defined in final ELF' '[[:space:]][Tt][[:space:]]+__wrap_nvs_flash_init$' "$SYMBOLS"
require_match 'partition-erase wrapper is defined in final ELF' '[[:space:]][Tt][[:space:]]+__wrap_esp_partition_erase_range$' "$SYMBOLS"
require_match 'initArduino calls the NVS-init wrapper' '<__wrap_nvs_flash_init([+>]|$)' "$CORE"
require_match 'initArduino calls the partition-erase wrapper' '<__wrap_esp_partition_erase_range([+>]|$)' "$CORE"
require_match 'NVS-init wrapper delegates to the real IDF function' '<nvs_flash_init([+>]|$)' "$WRAP_NVS"

# Xtensa has variable-length instructions. A linear disassembly can lose
# alignment after a branch. Decode each internal wrapper branch target afresh.
require_erase_delegate() {
  local targets target start_num stop_num start_arg stop_arg block
  targets="$(grep -Eo '[[:xdigit:]]+[[:space:]]+<__wrap_esp_partition_erase_range\+[^>]+>' \
    <<<"$WRAP_ERASE" | awk '{print $1}' | sort -u)"
  if [[ -z "$targets" ]]; then
    printf 'FAIL: partition-erase wrapper has no inspectable internal branch targets\n' >&2
    return 1
  fi

  while IFS= read -r target; do
    [[ -n "$target" ]] || continue
    start_num=$((16#$target))
    stop_num=$((start_num + 32))
    printf -v start_arg '0x%x' "$start_num"
    printf -v stop_arg '0x%x' "$stop_num"
    block="$("$OBJDUMP" -dC --start-address="$start_arg" --stop-address="$stop_arg" "$ELF")"
    if grep -Eq 'call[[:alnum:]]*[[:space:]].*<esp_partition_erase_range([+>]|$)' <<<"$block"; then
      printf 'PASS: partition-erase wrapper delegates to the real IDF function (branch target %s)\n' "$start_arg"
      return 0
    fi
  done <<<"$targets"

  printf 'FAIL: partition-erase wrapper delegates to the real IDF function\n' >&2
  return 1
}

require_erase_delegate

printf '\nPROBE_WRAPPER_ROUTING=PASS\n'
printf 'ELF=%s\n' "$ELF"
printf 'This verifies linked call routing only; it does not verify device behavior.\n'
