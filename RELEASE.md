# RELEASE v0.2.0 (2026-11-04)

Scope: hardware-free core, host tests, and the bring-up sketch. Not a production firmware.

## Contents
- lib/core/src: handoff core + data layer (config_store, month_set, roster_store, backup_format), API logic, enrolled roster, Core B fixes
- test/: 10 native suites, 149 test cases (sandbox), 142 device-confirmed before the template-backup cut and test_enroll
- sketch/sketch_compact_minimal_fixed.ino: bring-up diagnostic (ArduinoDroid, USB flashing)
- docs/: CONTEXT (start here), CRITICAL, DECISIONS, TESTS, UNVERIFIED, BRINGUP, SPECS, CHANGELOG, patches (provenance only)
- HANDOFF_FINAL.md: signed-off handoff

## Verification at release
- Checksums: MANIFEST.sha256 (every file except itself), and SHA256SUMS.txt beside the archive
- Extracted into a clean directory: all checksums match, git clean, 149 test cases pass in the sandbox stand-in harness
- NOT verified here: PlatformIO/Unity on the device with this exact tree (run it)

## Known blockers for production (see docs/CRITICAL.md)
C1 engine config link, C2 restore, C3 enrollment, C4 threshold data, C5 sensor duplicate, C6 security duties, C7 device test, C8 soak, C9 spec gaps (future-day rule, ID/page), C11 decision review, C12 KDF, C13 LittleFS

## Install
Replace the whole att/ tree. Do not merge single files. Do not run apply.sh (already applied). Then: pio test -e native 2>&1 | tail -14
