#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mode="${1:-targeted}"
actual_cpp="$(grep -R --include='*.cpp' -hE '^[[:space:]]*RUN_TEST\(' test | wc -l | tr -d ' ')"
actual_suites="$(find test -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"

if [[ "$actual_cpp" != "606" || "$actual_suites" != "35" ]]; then
  printf 'Inventory mismatch: C++ registrations=%s (expected 606), suites=%s (expected 35)\n' "$actual_cpp" "$actual_suites" >&2
  exit 1
fi

case "$mode" in
  manifest)
    sha256sum -c MANIFEST.sha256
    ;;
  targeted)
    sha256sum -c MANIFEST.sha256
    pio test -e native \
      -f 'test_api' \
      -f 'test_pin_auth_session'
    ;;
  *)
    echo "Usage: $0 [manifest|targeted]" >&2
    exit 2
    ;;
esac
