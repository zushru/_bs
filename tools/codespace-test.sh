#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

printf '\n=== Native C++ PlatformIO tests ===\n'
pio test -e native

printf '\n=== SF2 browser-reference-model tests ===\n'
npm --prefix web/sf2 test

printf '\n=== SF2 JavaScript syntax checks ===\n'
find web/sf2 -type f -name '*.mjs' -print0 | xargs -0 -r -n1 node --check

printf '\nALL_HOST_CHECKS=PASS\n'
