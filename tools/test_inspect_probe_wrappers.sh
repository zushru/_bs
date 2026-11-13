#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

TOOL_DIR="$TMP/home/.platformio/packages/toolchain-xtensa-esp-elf/bin"
mkdir -p "$TOOL_DIR"
touch "$TMP/good-smoke.elf" "$TMP/good-probe.elf"

cat > "$TOOL_DIR/xtensa-esp32-elf-nm" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
mode="${INSPECT_FIXTURE_MODE:-good}"
printf '40000000 T __wrap_nvs_flash_init\n'
if [[ "$mode" == "missing_wrapper_symbol" ]]; then
  exit 0
fi
printf '40000020 T __wrap_esp_partition_erase_range\n'
printf '40000040 T nvs_flash_erase\n'
printf '40000060 T nvs_flash_erase_partition\n'
printf '40000080 T nvs_flash_erase_partition_ptr\n'
STUB

cat > "$TOOL_DIR/xtensa-esp32-elf-objdump" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
mode="${INSPECT_FIXTURE_MODE:-good}"
symbol=""
start_address=""
for arg in "$@"; do
  case "$arg" in
    --disassemble=*) symbol="${arg#--disassemble=}" ;;
    --start-address=*) start_address="${arg#--start-address=}" ;;
  esac
done
if [[ "$start_address" == "0x400002a0" ]]; then
  if [[ "$mode" == "branch_target_erase_delegate" ]]; then
    printf '400002a0: call8 40000340 <esp_partition_erase_range>\\n'
  fi
  exit 0
fi
case "$symbol" in
  initArduino)
    if [[ "$mode" != "missing_init_route" ]]; then
      printf '40000100: call8 40000200 <__wrap_nvs_flash_init>\n'
    fi
    if [[ "$mode" != "missing_arduino_erase_route" ]]; then
      printf '40000104: call8 40000240 <__wrap_esp_partition_erase_range>\n'
    fi
    ;;
  __wrap_nvs_flash_init)
    if [[ "$mode" != "missing_init_delegate" ]]; then
      printf '40000200: call8 40000300 <nvs_flash_init>\n'
    fi
    ;;
  __wrap_esp_partition_erase_range)
    if [[ "$mode" == "branch_target_erase_delegate" ||           "$mode" == "missing_branch_target_erase_delegate" ]]; then
      printf '40000240: 589c beqz.n a8, 400002a0 <__wrap_esp_partition_erase_range+0x60>\n'
    elif [[ "$mode" != "missing_erase_delegate" ]]; then
      printf '40000240: call8 40000340 <esp_partition_erase_range>\n'
    fi
    ;;
  nvs_flash_erase)
    if [[ "$mode" == "direct_default_erase_route" ]]; then
      printf '40000400: call8 40000240 <__wrap_esp_partition_erase_range>\n'
    else
      printf '40000400: call8 40000460 <nvs_flash_erase_partition>\n'
    fi
    ;;
  nvs_flash_erase_partition)
    if [[ "$mode" == "missing_standard_erase_route" ]]; then
      printf '40000460: call8 40000340 <esp_partition_erase_range>\n'
    else
      printf '40000460: call8 40000240 <__wrap_esp_partition_erase_range>\n'
    fi
    ;;
  nvs_flash_erase_partition_ptr)
    if [[ "$mode" == "missing_pointer_erase_route" ]]; then
      printf '40000480: call8 40000340 <esp_partition_erase_range>\n'
    else
      printf '40000480: call8 40000240 <__wrap_esp_partition_erase_range>\n'
    fi
    ;;
  *)
    printf 'fixture: unrecognized disassembly target: %s\n' "$symbol" >&2
    exit 2
    ;;
esac
STUB

chmod +x "$TOOL_DIR/xtensa-esp32-elf-nm" \
  "$TOOL_DIR/xtensa-esp32-elf-objdump"

run_inspector() {
  local mode="$1"
  local output="$2"
  HOME="$TMP/home" INSPECT_FIXTURE_MODE="$mode" \
    "$ROOT/tools/inspect_probe_wrappers.sh" \
    "$TMP/good-smoke.elf" "$TMP/good-probe.elf" >"$output" 2>&1
}

expect_pass() {
  local mode="${1:-good}"
  local output="$TMP/${mode}.positive.log"
  if ! run_inspector "$mode" "$output"; then
    cat "$output" >&2
    printf 'FAIL: inspector rejected the positive fixture: %s\n' "$mode" >&2
    exit 1
  fi
  if ! grep -q '^NVS_WRAPPER_ROUTING=PASS$' "$output"; then
    cat "$output" >&2
    printf 'FAIL: positive fixture did not report routing pass: %s\n' "$mode" >&2
    exit 1
  fi
  printf 'PASS: inspector accepts %s\n' "$mode"
}

expect_fail() {
  local mode="$1"
  local expected="$2"
  local output="$TMP/${mode}.log"
  if run_inspector "$mode" "$output"; then
    cat "$output" >&2
    printf 'FAIL: inspector accepted negative fixture: %s\n' "$mode" >&2
    exit 1
  fi
  if grep -q '^NVS_WRAPPER_ROUTING=PASS$' "$output"; then
    cat "$output" >&2
    printf 'FAIL: inspector printed PASS for negative fixture: %s\n' "$mode" >&2
    exit 1
  fi
  if ! grep -q "$expected" "$output"; then
    cat "$output" >&2
    printf 'FAIL: inspector failed for the wrong reason: %s\n' "$mode" >&2
    exit 1
  fi
  printf 'PASS: inspector rejects %s\n' "$mode"
}

expect_pass
expect_pass direct_default_erase_route
expect_pass branch_target_erase_delegate
expect_fail missing_branch_target_erase_delegate 'partition-erase wrapper delegates to the real IDF function'
expect_fail missing_wrapper_symbol 'partition-erase wrapper is defined in final ELF'
expect_fail missing_init_route 'initArduino calls the NVS-init wrapper'
expect_fail missing_arduino_erase_route 'initArduino calls the partition-erase wrapper'
expect_fail missing_init_delegate 'NVS-init wrapper delegates to the real IDF function'
expect_fail missing_erase_delegate 'partition-erase wrapper delegates to the real IDF function'
expect_fail missing_standard_erase_route 'named-partition NVS erase reaches the wrapped boundary'
expect_fail missing_pointer_erase_route 'partition-pointer NVS erase reaches the wrapped boundary'

printf 'INSPECTOR_FIXTURES=PASS\n'
