#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mode="${1:-targeted}"
actual_cpp="$(grep -R --include='*.cpp' -hE '^[[:space:]]*RUN_TEST\(' test | wc -l | tr -d ' ')"
actual_suites="$(find test -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"

if [[ "$actual_cpp" != "601" || "$actual_suites" != "35" ]]; then
  printf 'Inventory mismatch: C++ registrations=%s (expected 601), suites=%s (expected 35)\n' "$actual_cpp" "$actual_suites" >&2
  exit 1
fi

case "$mode" in
  manifest)
    sha256sum -c MANIFEST.sha256
    ;;
  targeted)
    sha256sum -c MANIFEST.sha256
    pio test -e native \
      -f 'test_roster' \
      -f 'test_roster_history' \
      -f 'test_api' \
      -f 'test_enroll' \
      -f 'test_as608_pipeline' \
      -f 'test_coreb' \
      -f 'test_coreb2' \
      -f 'test_roster_snapshot' \
      -f 'test_month_roster' \
      -f 'test_restore_replay' \
      -f 'test_hardening' \
      -f 'test_attendance_admission'
    ;;
  *)
    echo "Usage: $0 [manifest|targeted]" >&2
    exit 2
    ;;
esac
