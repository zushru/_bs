#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
if ! command -v "$CXX" >/dev/null 2>&1; then
  printf 'ERROR: host C++ compiler not found: %s\n' "$CXX" >&2
  exit 127
fi

BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/att-host-compile.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

cd "$ROOT"
INCLUDES=(
  "-I$ROOT/lib/core/src"
  "-I$ROOT/lib/as608/src"
  "-I$ROOT/lib/hal_esp32/src"
  "-I$ROOT/lib/littlefs_file_set/src"
)
while IFS= read -r -d '' stub_dir; do
  INCLUDES+=("-I$ROOT/$stub_dir")
done < <(find test -type d -name stubs -print0 | sort -z)

mapfile -d '' SOURCES < <(find lib -type f -name '*.cpp' -print0 | sort -z)
if ((${#SOURCES[@]} == 0)); then
  printf 'ERROR: no production C++ translation units found under lib/\n' >&2
  exit 2
fi

printf 'HOST_COMPILE_BEGIN compiler=%s standard=gnu++11 units=%d\n' \
  "$("$CXX" --version | head -n 1)" "${#SOURCES[@]}"
for index in "${!SOURCES[@]}"; do
  src="${SOURCES[$index]}"
  object_name="${src//\//__}"
  object_name="${object_name%.cpp}.o"
  printf '[%d/%d] %s\n' "$((index + 1))" "${#SOURCES[@]}" "$src"
  "$CXX" -std=gnu++11 -Wall -Wextra "${INCLUDES[@]}" \
    -c "$src" -o "$BUILD_DIR/$object_name"
done
printf 'HOST_COMPILE_PASS units=%d\n' "${#SOURCES[@]}"
printf 'SCOPE=host_object_compilation_only tests_not_run target_not_built\n'
