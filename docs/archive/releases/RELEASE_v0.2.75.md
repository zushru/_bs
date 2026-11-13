# Release v0.2.75 — fail-closed NVS config boot gate

Date: 2026-11-10  
Baseline: v0.2.74  
Status: development snapshot; not production firmware and not hardware-verified.

## Scope

C34/C1 startup-boundary increment. Esp32NvsConfigBackend::begin() now consumes the pre-setup Esp32NvsStartupGuard snapshot. It refuses to mark the backend ready unless the snapshot says exactly one successful Arduino NVS initialization occurred with no blocked whole-partition erase attempt. It returns the original startup error when available and uses ESP_FAIL for an unsafe snapshot whose first init result was ESP_OK.

The backend no longer calls nvs_flash_init() a second time. ConfigBootCoordinator::start() latches CFG_BOOT_FAULT before invoking the backend, so config loading, AP setup and PIN runtime startup cannot proceed through their owning coordinators after this gate fails.

No erase policy, format path, recovery protocol, stored format, or signed/delegated decision changed.

## Changed source and tests

- lib/hal_esp32/src/nvs_config_backend.cpp
- lib/hal_esp32/src/nvs_config_backend.h
- test/test_nvs_config/test_main.cpp
- Added tools/verify_v0.2.75.sh
- Added docs/AUDIT_v0.2.75.md
- Updated RELEASE.md, README.md, MANIFEST.sha256, docs/CHANGELOG.md, docs/TESTS.md, docs/CONTEXT.md, docs/CRITICAL.md, docs/UNVERIFIED.md, docs/DEVELOPMENT_ROADMAP.md and docs/WORKING_MEMORY.md for the current source, test evidence and remaining limits.

Two new native test registrations cover an ambiguous repeated Arduino initialization and the successful already-initialized path with no second init. Existing error cases were adapted to represent the captured Arduino startup snapshot. Live inventory is 630 C++ registrations across 35 suites; SF2 inventory remains 257 across 12 files.

## Verification

Focused native commands:

    pio test -e native -f 'test_nvs_config'
    pio test -e native -f 'test_nvs_startup_guard_policy'

Results:
- test_nvs_config: 46/46, exit 0.
- test_nvs_startup_guard_policy: 9/9, exit 0.
- Total selected: 55/55 across two distinct suites.

Test-first evidence: the second red run reproduced all four intended backend failures. PlatformIO's test process emitted SIGILL after the failing Unity summary; this was not counted as a normal red completion. The green owning run completed normally with exit 0. The first red run also exposed a fixture contamination in the existing “load before NVS init” test; the fixture was corrected to model an uninitialized driver explicitly.

Pinned target build:

    pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

- PlatformIO result: SUCCESS; build log reports 2,086.96 seconds.
- RAM: 67,356 / 327,680 bytes (20.6%).
- Application flash: 935,040 / 1,310,720 bytes (71.3%).
- ELF: 11,192,840 bytes; SHA-256 1e959f325be97ee57c13eb852d40bdc40666cbbaceace8933f56a83d9c0ac57e.
- BIN: 944,112 bytes; SHA-256 dfc8c198d2dbd90aa1be6281cf99d6f4179ffbd14fd7072e8232ef95c968420d.

Final-ELF checks:
- tools/inspect_probe_wrappers.sh: 12/12 NVS wrapper-routing checks passed.
- New verifier: retains Esp32NvsConfigBackend::begin() and Esp32NvsStartupGuard::status().
- New verifier: retains all 9 LittleFsFileSet public methods.

These are host-test, compile/link and static-ELF results only. The inert smoke image was not flashed.

## Deliberately not run

- Full pio test -e native — N1 remains deferred until the agreed integration checkpoint.
- SF2 npm test — no SF2 source changed.
- Hardware-probe build/run, flashing, runtime NVS access, NVS corruption, LittleFS mount/VFS operations, format/erase, sensor tests, brownout, power-cut, powerbank or physical-device tests.

## Remaining blockers and limits

C34 remains blocking. The existing erase wrapper is still narrow: chunked ranges, DATA_NVS_KEYS and direct lower-level flash erase APIs are not a complete-firewall-safe scope. This increment adds a gate at the config backend boundary, not a production firmware root. The AP-credential and PIN NVS stores remain low-level adapters whose safe order depends on their owning coordinators being reached only after successful ConfigBootCoordinator::start(). The future production root must establish that order before any direct NVS consumer or other application flash/storage owner starts.

There is no production main/scheduler, integrated HTTP/auth boundary, storage mount owner or hardware evidence. The C13.2 recovery proposal remains NOT APPROVED FOR IMPLEMENTATION. No product decision or protected source changed.
