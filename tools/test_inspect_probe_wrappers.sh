#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

BIN="$TMP/home/.platformio/packages/toolchain-xtensa-esp-elf/bin"
mkdir -p "$BIN"
printf 'fixture ELF\n' > "$TMP/firmware.elf"

cat > "$BIN/xtensa-esp32-elf-nm" <<'MOCK_NM'
#!/usr/bin/env bash
cat <<'SYMBOLS'
00001000 T __wrap_nvs_flash_init
00001100 T __wrap_esp_partition_erase_range
00001200 T initArduino
00001300 T nvs_flash_init
00001400 T esp_partition_erase_range
SYMBOLS
MOCK_NM

cat > "$BIN/xtensa-esp32-elf-objdump" <<'MOCK_OBJDUMP'
#!/usr/bin/env bash
args="$*"
case "$args" in
  *--disassemble=initArduino*)
    cat <<'DISASM'
00001200 <initArduino>:
  1200: call8 00001000 <__wrap_nvs_flash_init>
  1203: call8 00001100 <__wrap_esp_partition_erase_range>
DISASM
    ;;
  *--disassemble=__wrap_nvs_flash_init*)
    cat <<'DISASM'
00001000 <__wrap_nvs_flash_init>:
  1000: call8 00001300 <nvs_flash_init>
DISASM
    ;;
  *--disassemble=__wrap_esp_partition_erase_range*)
    cat <<'DISASM'
00001100 <__wrap_esp_partition_erase_range>:
  1118: beqz.n a8, 00001180 <__wrap_esp_partition_erase_range+0x80>
  111b: .byte 0xc7
  111c: .byte 0xa2
DISASM
    ;;
  *--start-address=0x1180*)
    if [[ "${MOCK_MISSING_ERASE_DELEGATE:-0}" == 1 ]]; then
      cat <<'DISASM'
00001180 <__wrap_esp_partition_erase_range+0x80>:
  1180: retw.n
DISASM
    else
      cat <<'DISASM'
00001180 <__wrap_esp_partition_erase_range+0x80>:
  1180: call8 00001400 <esp_partition_erase_range>
DISASM
    fi
    ;;
  *)
    printf 'unexpected objdump invocation: %s\n' "$args" >&2
    exit 90
    ;;
esac
MOCK_OBJDUMP

chmod +x "$BIN/xtensa-esp32-elf-nm" "$BIN/xtensa-esp32-elf-objdump"

if ! HOME="$TMP/home" "$ROOT/tools/inspect_probe_wrappers.sh" "$TMP/firmware.elf" \
  > "$TMP/pass.log" 2>&1; then
  cat "$TMP/pass.log" >&2
  exit 1
fi
grep -q '^PROBE_WRAPPER_ROUTING=PASS$' "$TMP/pass.log"

if HOME="$TMP/home" MOCK_MISSING_ERASE_DELEGATE=1 \
  "$ROOT/tools/inspect_probe_wrappers.sh" "$TMP/firmware.elf" \
  > "$TMP/fail.log" 2>&1; then
  echo 'FAIL: inspector accepted a wrapper without real erase delegation' >&2
  exit 1
fi
grep -q 'FAIL: partition-erase wrapper delegates to the real IDF function' \
  "$TMP/fail.log"

echo 'PASS: probe wrapper inspector follows Xtensa branch targets and fails closed'
