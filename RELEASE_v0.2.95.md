# Development release v0.2.95 — 2026-11-12

## Yield

Adds a cross-layer native regression for an `IFileSet::addFile()` failure that occurs after the empty month entry has become visible. The test drives the real `MonthSet` and `LogStore` over a faulting host fake. It verifies that the append returns false, `LogStore` latches its fault, no event bytes are sent to `appendFile`, a second append is refused, and a fresh `open()` scan must reconcile the empty file before a later append is accepted.

No production behavior or recovery policy changed. The regression distinguishes this pre-payload file-creation failure from an indeterminate `appendFile()` failure, where record bytes may already exist.

## Baseline

- Source tree: `/root/att`, v0.2.94; no Git metadata.
- Pre-edit manifest: 449 entries, all verified.
- Protected files unchanged: `HANDOFF_FINAL.md` SHA-256 `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`; `docs/DECISIONS.md` SHA-256 `52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3`.

## Changed source/test file

- `test/test_monthset/test_main.cpp`

## Verification

`pio test -e native -f 'test_monthset' -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'`

Result: **100/100 native test cases passed** in 70.716 seconds. Exact log: `/tmp/att_v0295_c13_native.log`.

## Not run / evidence still required

- No production target source changed; the v0.2.94 inert ESP32 compile/link result was not rerun and remains compile/link-only evidence.
- No mounted ESP-IDF LittleFS VFS operation, actual descriptor lifecycle, low-space/near-full fault, parent-directory entry durability, reset/interruption matrix, brownout, or physical power-cut test was performed.
- No production root wires this storage owner into the attendance application. C13.3 remains BLOCKING.
- No format, truncate, quarantine, repair, journal, restore swap, or destructive recovery behavior was enabled.
- `MonthSet::repair()` remains non-transactional and unavailable to production callers. No LittleFS commit primitive is proven or accepted.
