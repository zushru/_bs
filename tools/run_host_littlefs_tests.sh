#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
if ! command -v "$CXX" >/dev/null 2>&1; then
  printf 'ERROR: C++ compiler not found: %s\n' "$CXX" >&2
  exit 127
fi

BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/att-lfs-tests.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

FLAGS=(-std=gnu++11 -Wall -Wextra -Werror)
if [[ "${SANITIZE:-0}" == "1" ]]; then
  FLAGS+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
fi
INCLUDES=(
  "-I$ROOT/tools/host_test_support"
  "-I$ROOT/lib/core/src"
  "-I$ROOT/lib/as608/src"
  "-I$ROOT/lib/hal_esp32/src"
  "-I$ROOT/lib/littlefs_file_set/src"
)

printf 'HOST_LITTLEFS_TESTS_BEGIN compiler=%s sanitize=%s\n' \
  "$("$CXX" --version | head -n 1)" "${SANITIZE:-0}"

"$CXX" "${FLAGS[@]}" "${INCLUDES[@]}" \
  "$ROOT/test/test_littlefs_fileset/test_main.cpp" \
  "$ROOT/lib/littlefs_file_set/src/littlefs_file_set.cpp" \
  "$ROOT"/lib/core/src/*.cpp \
  -o "$BUILD_DIR/littlefs_fileset"
"$BUILD_DIR/littlefs_fileset"

"$CXX" "${FLAGS[@]}" "${INCLUDES[@]}" \
  "$ROOT/test/test_littlefs_serialized_fileset/test_main.cpp" \
  "$ROOT/lib/littlefs_file_set/src/littlefs_serialized_fileset.cpp" \
  -o "$BUILD_DIR/littlefs_serialized_fileset"
"$BUILD_DIR/littlefs_serialized_fileset"

"$CXX" "${FLAGS[@]}" "${INCLUDES[@]}" \
  "$ROOT/test/test_littlefs_mount_owner/test_main.cpp" \
  "$ROOT/lib/littlefs_file_set/src/littlefs_mount_owner.cpp" \
  "$ROOT/lib/littlefs_file_set/src/littlefs_serialized_fileset.cpp" \
  "$ROOT/lib/littlefs_file_set/src/littlefs_file_set.cpp" \
  "$ROOT/lib/hal_esp32/src/esp32_littlefs_mount.cpp" \
  "$ROOT"/lib/core/src/*.cpp \
  -o "$BUILD_DIR/littlefs_mount_owner"
"$BUILD_DIR/littlefs_mount_owner"

"$CXX" "${FLAGS[@]}" "${INCLUDES[@]}" \
  -I"$ROOT/target/esp32_hardware_probe" \
  "$ROOT/test/test_littlefs_vfs_probe/test_main.cpp" \
  "$ROOT/target/esp32_hardware_probe/littlefs_vfs_probe.cpp" \
  -o "$BUILD_DIR/littlefs_vfs_probe"
"$BUILD_DIR/littlefs_vfs_probe"

printf 'HOST_LITTLEFS_TESTS_PASS suites=4 tests=79 scope=POSIX_host_not_target_LittleFS\n'
