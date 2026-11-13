#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mode="${1:-targeted}"
if [[ "$mode" != "targeted" ]]; then
  printf 'Usage: %s [targeted]\n' "$0" >&2
  exit 2
fi

printf 'Source root: %s\n' "$ROOT"
printf 'Mode: %s\n' "$mode"
printf 'This workflow never uploads or flashes.\n'

sha256sum -c MANIFEST.sha256
pio test -e native \
  -f 'test_littlefs_fileset' \
  -f 'test_monthset'

printf '\nTARGETED_V0_2_57=PASS\n'
