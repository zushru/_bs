# Critical audit — v0.2.36

Date: 2026-11-07  
Scope: correct the v0.2.35 `test_nvs_config` read-I/O failure fixture  
Baseline: v0.2.35, SHA-256 `527cb2ab9a65bcd8376c8e74d2d8af9005eb7a24b66fb75909f99e31d2385aab`

## 1. User-supplied test evidence

The user pasted a focused `test_nvs_config` run with 44 tests, 1 failure, and 0 ignored. The other 43 listed cases passed. The failed case was `test_pin_auth_runtime_does_not_initialize_when_nvs_read_has_io_error`, reporting `Expected 4 Was 5`, followed by `SIGHUP` and a PlatformIO `ERRORED` status.

The expected value 4 is `EPAR_COUNTER_READ_FAILED`; 5 is `EPAR_COUNTER_INITIALIZE_FAILED`.

## 2. Root cause

The failing fixture set the global fake `getResult = ESP_FAIL` but did not mark the fake `attsec` namespace as present. `nvs_open("attsec", NVS_READONLY, ...)` therefore returned `ESP_ERR_NVS_NOT_FOUND` before `nvs_get_u16()` was called. The runtime correctly treated this as a missing counter during verified `NEEDS_SETUP` and attempted the explicit initialization path. That path then encountered the injected `ESP_FAIL` and returned `EPAR_COUNTER_INITIALIZE_FAILED`.

The test name and intent were to exercise an I/O error from reading the existing namespace's counter key. The fixture did not model that state. This is a test-fixture defect, not evidence of a production runtime logic defect.

## 3. Correction

`test/test_nvs_config/test_main.cpp` now marks `pinNamespaces[0].exists = true` while leaving `u16Present` false, then injects `ESP_FAIL`. This allows `nvs_open(READONLY)` to succeed and forces the error at `nvs_get_u16()`.

The test was renamed to `test_pin_auth_runtime_does_not_initialize_when_counter_read_has_io_error` and now asserts:

- one namespace open;
- one NVS get call;
- no `nvs_set_u16()` call;
- one `nvs_close()` call;
- runtime latches fault and exposes no coordinator;
- no `nvs_set_u16()` or `nvs_commit()` call;
- runtime reports `EPAR_COUNTER_READ_FAILED` and preserves `ESP_FAIL` as its last store error.

No production implementation, test registration count, policy, decision, hardware mapping, or storage format changed.

## 4. Verification status

- v0.2.35 focused run: **44 tests, 1 failed, 0 ignored**, as pasted by the user.
- v0.2.36 source change: reviewed only; no compiler or PlatformIO run was performed here.
- SIGHUP: appears after the single assertion failure; the excerpt does not establish whether it is a separate runner defect.
- Full native regression, Node tests, ESP32 target build and device tests remain unverified.

## 5. Next verification

From a clean full v0.2.36 extraction, run the focused `test_nvs_config` suite, then the full native suite, then `(cd web/sf2 && npm test)`. Preserve process exit codes and complete summaries. Do not continue to dependent C6 modules until the focused and full host checkpoints are clean.

## Verdict

The v0.2.35 failure is explained as a fake-NVS fixture setup defect. The v0.2.36 test correction targets the intended read-error path without changing production code. It remains unverified until the user reruns it.
