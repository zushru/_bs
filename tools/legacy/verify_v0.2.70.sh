#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mode="${1:-targeted}"
actual_cpp="$(grep -R --include='*.cpp' -hE '^[[:space:]]*RUN_TEST\(' test | wc -l | tr -d ' ')"
actual_suites="$(find test -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"

if [[ "$actual_cpp" != "621" || "$actual_suites" != "35" ]]; then
  printf 'Inventory mismatch: C++ registrations=%s (expected 621), suites=%s (expected 35)\n' "$actual_cpp" "$actual_suites" >&2
  exit 1
fi

case "$mode" in
  manifest)
    sha256sum -c MANIFEST.sha256
    ;;
  targeted)
    sha256sum -c MANIFEST.sha256
    bash -n tools/inspect_probe_wrappers.sh tools/verify_v0.2.70.sh
    test -x tools/inspect_probe_wrappers.sh
    printf 'v0.2.70 manifest, inventory and inspector tooling: PASS\n'
    ;;
  elf)
    shift
    if [[ "$#" -eq 0 ]]; then
      echo "Usage: $0 elf <firmware.elf> [firmware.elf ...]" >&2
      exit 2
    fi
    tools/inspect_probe_wrappers.sh "$@"
    ;;
  *)
    echo "Usage: $0 [manifest|targeted|elf <firmware.elf> ...]" >&2
    exit 2
    ;;
esac
