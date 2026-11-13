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
  -f 'test_monthset' \
  -f 'test_restore_replay' \
  -f 'test_enroll' \
  -f 'test_api' \
  -f 'test_roster' \
  -f 'test_roster_snapshot' \
  -f 'test_event_group_audit' \
  -f 'test_roster_history' \
  -f 'test_coreb' \
  -f 'test_backup' \
  -f 'test_coreb2' \
  -f 'test_hardening' \
  -f 'test_attendance_admission' \
  -f 'test_month_roster' \
  -f 'test_as608_pipeline'

printf '\nTARGETED_V0_2_56=PASS\n'
