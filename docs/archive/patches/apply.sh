#!/bin/sh
# Run from the att/ repo root after extracting the tarball.
set -e
[ -f .apply_done ] && echo "(re-running: baseline files were restored by the tarball)"
git apply --check core_fixes.patch || { echo "PATCH DOES NOT APPLY: already applied, or your core files differ from the handoff. If this is the first run, stop and report."; exit 1; }
git apply core_fixes.patch
python3 fix_b2_tests.py
echo "applied. now: pio test -e native 2>&1 | tail -14"
