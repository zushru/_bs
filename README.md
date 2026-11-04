# att: ESP32 fingerprint attendance, v0.2.0 (2026-11-04)

**Official release: v0.2.0 (2026-11-04). Use only this archive.** It supersedes att_data_layer.tar.gz, the first snapshot and r2. Replace your whole att/ tree with it. Do not merge single files, and do not run apply.sh (already applied here). See docs/CRITICAL.md C14.

Start with docs/CONTEXT.md. Read docs/CRITICAL.md before building anything. Decisions: docs/DECISIONS.md.

## Layout
- lib/core/src: hardware-free core in C++ (handoff modules, data layer, API logic, enrolled roster)
- test/: 10 native suites (PlatformIO env `native`, Unity)
- sketch/: bring-up diagnostic (ArduinoDroid). Not production firmware
- docs/: context, critical issues, decisions, specs, tests, unverified claims, bring-up record
- HANDOFF_FINAL.md: signed-off handoff (product rules and architecture)
- MANIFEST.sha256: checksums of every file in this snapshot
- git: a single commit on branch main (no history). The tree is the release.

## Quick start (Termux + proot Debian)
    cd att
    pio test -e native 2>&1 | tail -14
Expected: 149 test cases: 149 succeeded. Device-confirmed so far: 142 (see docs/TESTS.md). Treat 149 as unconfirmed until you run it.

## Verify this archive
    sha256sum -c MANIFEST.sha256
    git log --oneline
    git status --short
