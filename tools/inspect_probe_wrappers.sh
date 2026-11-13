#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAIN_DIR="${HOME}/.platformio/packages/toolchain-xtensa-esp-elf/bin"

if (($#)); then
  ELFS=("$@")
else
  # Separate PlatformIO configs share .pio/build and the later build can
  # replace the earlier ELF. Inspect every image currently present; the
  # verification workflow inspects each target immediately after its build.
  ELFS=()
  for candidate in \
    "$ROOT/.pio/build/esp32-adapter-link/firmware.elf" \
    "$ROOT/.pio/build/esp32-hardware-probe/firmware.elf"; do
    [[ -f "$candidate" ]] && ELFS+=("$candidate")
  done
  if (("${#ELFS[@]}" == 0)); then
    printf 'ERROR: no target ELF found. Build a target first.\n' >&2
    exit 2
  fi
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

NM="$(find_tool xtensa-esp32-elf-nm)"
OBJDUMP="$(find_tool xtensa-esp32-elf-objdump)"

# Xtensa has variable-length instructions. Linear objdump from a function entry
# can encounter data/alternate instruction boundaries after an unconditional
# branch and miss the real basic block. Recursively inspect branch labels that
# objdump identifies as offsets within the same function.
disassemble_function() {
  local elf="$1"
  local symbol="$2"
  local output fragment target stop iteration
  output="$("$OBJDUMP" -dC --disassemble="$symbol" "$elf")"
  local seen=" "
  for iteration in 1 2 3 4 5 6 7 8; do
    local added=0
    local targets
    targets="$(grep -oE "[[:xdigit:]]+[[:space:]]+<${symbol}\\+0x[[:xdigit:]]+>" \
      <<<"$output" | sed -E 's/[[:space:]].*$//' | sort -u || true)"
    while IFS= read -r target; do
      [[ -n "$target" ]] || continue
      case "$seen" in
        *" $target "*) continue ;;
      esac
      seen+="$target "
      stop=$((16#$target + 96))
      fragment="$("$OBJDUMP" -dC --start-address="0x$target" \
        --stop-address="$(printf '0x%x' "$stop")" "$elf")"
      output+=$'\n'"$fragment"
      added=1
    done <<<"$targets"
    (( added == 0 )) && break
  done
  printf '%s\n' "$output"
}

check_elf() {
  local elf="$1"
  if [[ ! -f "$elf" ]]; then
    printf 'ERROR: ELF not found: %s\n' "$elf" >&2
    return 2
  fi

  printf '\nChecking NVS startup and erase routing: %s\n' "$elf"
  local symbols core wrap_nvs wrap_erase erase_default erase_partition erase_partition_ptr
  symbols="$("$NM" -C --defined-only "$elf")"
  core="$(disassemble_function "$elf" initArduino)"
  wrap_nvs="$(disassemble_function "$elf" __wrap_nvs_flash_init)"
  wrap_erase="$(disassemble_function "$elf" __wrap_esp_partition_erase_range)"
  erase_default="$(disassemble_function "$elf" nvs_flash_erase)"
  erase_partition="$(disassemble_function "$elf" nvs_flash_erase_partition)"
  erase_partition_ptr="$(disassemble_function "$elf" nvs_flash_erase_partition_ptr)"

  require_match 'NVS-init wrapper is defined in final ELF' \
    '[[:space:]][Tt][[:space:]]+__wrap_nvs_flash_init$' "$symbols"
  require_match 'partition-erase wrapper is defined in final ELF' \
    '[[:space:]][Tt][[:space:]]+__wrap_esp_partition_erase_range$' "$symbols"
  require_match 'initArduino calls the NVS-init wrapper' \
    '<__wrap_nvs_flash_init([+>]|$)' "$core"
  require_match 'initArduino calls the partition-erase wrapper' \
    '<__wrap_esp_partition_erase_range([+>]|$)' "$core"
  require_match 'NVS-init wrapper delegates to the real IDF function' \
    '<nvs_flash_init([+>]|$)' "$wrap_nvs"
  require_match 'partition-erase wrapper delegates to the real IDF function' \
    '<esp_partition_erase_range([+>]|$)' "$wrap_erase"

  require_match 'standard nvs_flash_erase API is retained for inspection' \
    '[[:space:]][Tt][[:space:]]+nvs_flash_erase$' "$symbols"
  require_match 'named-partition NVS erase API is retained for inspection' \
    '[[:space:]][Tt][[:space:]]+nvs_flash_erase_partition$' "$symbols"
  require_match 'partition-pointer NVS erase API is retained for inspection' \
    '[[:space:]][Tt][[:space:]]+nvs_flash_erase_partition_ptr$' "$symbols"

  if grep -Eq '<__wrap_esp_partition_erase_range([+>]|$)' <<<"$erase_default"; then
    printf 'PASS: nvs_flash_erase reaches the wrapped whole-partition boundary directly\n'
  else
    require_match 'nvs_flash_erase delegates to named-partition erase' \
      '<nvs_flash_erase_partition([+>]|$)' "$erase_default"
  fi
  require_match 'named-partition NVS erase reaches the wrapped boundary' \
    '<__wrap_esp_partition_erase_range([+>]|$)' "$erase_partition"
  require_match 'partition-pointer NVS erase reaches the wrapped boundary' \
    '<__wrap_esp_partition_erase_range([+>]|$)' "$erase_partition_ptr"
}

for elf in "${ELFS[@]}"; do
  check_elf "$elf"
done

printf '\nNVS_WRAPPER_ROUTING=PASS\n'
printf 'This proves linked call routing for the inspected paths only; it does not prove runtime or device behavior.\n'
