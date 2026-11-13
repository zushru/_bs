# ATT-P15-AP-START-EVENT-REG-01 — AP_START-only RF entropy gate

- Author: SHRU
- Status: COMPLETE — TEST SOURCE AUTHORED; NOT RUN
- Baseline: v0.2.95 development snapshot
- Workspace: CANONICAL_SHARED_TREE
- Authority: E28/E33; `Esp32WifiAp` and `Esp32SecureRandom` contracts
- Read: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md; `esp32_wifi_ap.h/.cpp`, `esp32_secure_random.h/.cpp`; assigned AP test harness and the existing event-group/event stubs.

## Change

Only `test/test_esp32_wifi_ap/test_main.cpp` was modified.

- Added one fake control, `emitInvalidApStartCallbacks`, to inject invalid event callbacks from the fake `esp_wifi_start()` while the actual AP_START event is suppressed.
- The fake injects three invalid shapes to the registered callback: an unrelated `WIFI_EVENT_AP_STOP`, `WIFI_EVENT_AP_START` under the wrong `IP_EVENT` base, and the correct base/ID with a null callback context.
- Added `test_invalid_ap_start_callbacks_do_not_open_rf_gate_and_timeout_cleanup_runs()`. It expects the existing `EWAP_START_TIMEOUT` result, a faulted/not-ready AP, `random.rfStarted()==false`, no successful random fill and zero calls to `esp_fill_random`, then checks the existing stop/unregister/deinit/event-group cleanup path completed.

No production Wi-Fi code, shared stubs, other test files or build files were changed.

## Provenance and static verification

- Pre-edit copy: `comms/staging/SHRU/ATT-P15-AP-START-EVENT-REG-01/before/test_main.cpp`
- Unified diff: `comms/staging/SHRU/ATT-P15-AP-START-EVENT-REG-01/patch.diff`
- Before SHA-256: `f9394980795539346739bbe08e68cbfe78eebbfc3fe4f2c77477d8151744f41f`
- After SHA-256: `ed299a97ceface21a039ad449a2eab2ecd29186171bd053833c95692a068247b`
- Patch SHA-256: `543290cfa72b9bf55fa09cf780a309e7c653827fa15d0506610add54098b0189`
- File read-back and saved-diff comparison completed.
- Static source consistency: 23 test definitions / 23 registrations, no duplicates or missing registrations; no trailing whitespace.

Tests, compiler, build, PlatformIO, flash and hardware: **NOT RUN**, as assigned. This does not establish physical RF behavior; it is a native fake-callback regression source only.

Next: PISHDA independently reviews the exact test and fake change before acceptance.
