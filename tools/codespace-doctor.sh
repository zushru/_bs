#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
printf 'Architecture: '; uname -m
printf 'Python: '; python3 --version
printf 'Node: '; node --version
printf 'npm: '; npm --version
printf 'PlatformIO: '; pio --version
printf 'Native test registrations: '
grep -Rho 'RUN_TEST[[:space:]]*(' test --include='*.cpp' | wc -l
printf 'SF2 test files: '
find web/sf2/test -type f -name '*.test.mjs' | wc -l
printf '\nUSB serial devices visible in Codespaces (normally none):\n'
find /dev -maxdepth 2 \( -name 'ttyUSB*' -o -name 'ttyACM*' \) -print 2>/dev/null || true
printf '\nCodespaces is for source editing, tests and compile/link. Flash locally.\n'
