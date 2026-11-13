# Critical audit — v0.2.34

Date: 2026-11-07  
Scope: NVS PIN-attempt persistence and boot ownership (C6)  
Baseline: v0.2.33, SHA-256 `a0a7e7ae57a1ca619ddc13127f819e99d60285720c0984684323c9ebda669937`

## 1. Changes made

### `Esp32NvsPinAttemptStore`

- Stores the attempt count as NVS `uint16_t`, key `pin_fails`, in a caller-selected namespace.
- Rejects null/empty namespaces, names longer than 15 characters, and `attcfg` (the existing config-slot namespace).
- Does not call `nvs_flash_init()`, erase, format or auto-repair NVS. The application boot path must have initialized NVS first.
- `loadFailureCount()` uses read-only access and only updates the caller's output on success.
- `persistFailureCount()` first requires an existing typed key, then sets, commits, closes and reads back through a newly opened read-only handle. A missing key, wrong type, failed set/commit/readback, or value mismatch returns false.
- `initializeFailureCount()` is an explicit first-provision operation. It writes zero only when the key is absent; it refuses an existing counter and does not overwrite a wrong-type entry.

### `Esp32PinAuthRuntime`

- Owns one store and the coordinator built from that exact store; both classes are non-copyable.
- Rejects startup unless `ConfigBootCoordinator` is already `READY` or `NEEDS_SETUP`.
- On `NEEDS_SETUP`, it loads first. A valid existing counter is preserved, including after an interrupted first-time setup. Only `ESP_ERR_NVS_NOT_FOUND` permits calling the explicit initializer. Wrong-type and other I/O errors fail closed.
- On `READY`, it does not initialize. The normal coordinator boot load must find existing counter state; otherwise startup faults.
- It is one-shot. The coordinator is exposed to caller code only after successful startup.

The runtime is a boot-composition helper, not a production firmware entry point. No HTTP route or production main currently invokes it.

## 2. Regression coverage added

24 `RUN_TEST` registrations in `test_nvs_config`:

- 16 store tests: pre-init load, NVS open/read errors, missing key, initialization and no-overwrite behavior, wrong type, commit/readback, set failure, commit ambiguity, missing-key persist, namespace separation, namespace validation and reserved namespace rejection.
- 8 boot-owner tests: config boot not ready, first-time initialization, interrupted setup count preservation, no auto-initialization on provisioned devices, provisioned load, wrong-type preservation, one-shot startup and non-NOT_FOUND read error handling.
- Compile-time assertions prohibit copying the store/runtime and pin the ESP-IDF NVS error-code values used by the host fake.

Current source inventory: 413 C++ `RUN_TEST` registrations across 23 suites; 257 SF2 Node registrations. Registration counts are not execution results.

## 3. Critical review findings / residual risks

1. **No tests were run for v0.2.34.** The 24 new cases have not been compiled or executed. The existing v0.2.33 all-pass status is user-reported without terminal output or exact counts.
2. **No target build exists in this verification.** The project has no production ESP32 entry point. Host stubs cannot establish that the selected Arduino-ESP32/ESP-IDF toolchain links the new adapter.
3. **NVS readback is not a brownout test.** Commit and readback check API-level state in the current process. Real flash behavior and controlled power interruption remain unmeasured.
4. **First-provision authorization remains caller responsibility.** The runtime keys initialization to `ConfigBootCoordinator::needsSetup()`, but a production composition root must call `ConfigBootCoordinator::start()` first, pass the same authoritative boot instance, and prevent any secret route from running until the runtime is ready.
5. **One-writer serialization is required.** Neither NVS adapter nor the coordinator implements a mutex or cross-task compare-and-swap. Use one runtime/coordinator per chosen namespace and serialize the entire auth operation if multiple tasks are ever introduced.
6. **Counter-domain allocation is unresolved.** E12 remains DELEGATED. The namespace parameter supports the eventual choice; this release does not select shared vs separate PIN/setup-code domains.
7. **Credential crypto is still absent.** PIN PBKDF2 iteration policy is not specified. Do not invent it. The backup KDF parameter C12 is a separate issue.
8. **No route boundary exists.** The runtime does not guarantee that API handlers use its coordinator. Direct calls to secret verification can still bypass lockout until every relevant route is wired and audited.
9. **The NVS key is not a security root.** Under signed R11, physical flash access is out of scope; no flash-encryption/tamper-resistance claim is made.

## 4. Source/API review

The adapter uses the ESP-IDF 5.5 NVS C API and error semantics. Espressif's reference documents `nvs_get_u16`, `nvs_set_u16`, the commit boundary, the `ESP_ERR_NVS_TYPE_MISMATCH` condition and 15-character namespace limit: <https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/storage/nvs_flash.html.

The fake header encodes `ESP_ERR_NVS_TYPE_MISMATCH` as `0x1103`, and the test translation unit asserts that value. This assertion protects the fake's assumed code, not the exact platform version selected by an eventual firmware build.

## 5. Policy and scope check

- E12 remains DELEGATED; no counter-domain selection was made.
- E21 remains OPEN; NVS attempt storage is only one adapter. LittleFS, HTTP and mbedTLS credential/backup adapters remain incomplete.
- O8 backup scope remains config + roster + log. No reporting PII persistence or template-backup scope was changed.
- R11 remains the signed network-only threat model. No physical-flash security is claimed.
- E27 and all SF2 decisions remain unchanged.
- No core attendance/event/storage format or hardware pin was changed.

## 6. Required user verification

From a clean extraction of the complete archive:

```sh
sha256sum -c MANIFEST.sha256
pio test -e native -f test_nvs_config -vvv
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

Report the focused suite summary, full suite summary and process exit status. Do not merge individual files. A successful host run should be followed by an exact target build and device tests; it still does not settle NVS power-loss behavior.

## Verdict

**The C6 persistence foundation is materially advanced, but C6 remains BLOCKING.** The v0.2.34 source has a safer NVS counter adapter and boot owner, but no test evidence yet, no production caller, no credential verifier/writer, no token/session action, and no HTTP enforcement. Do not treat this snapshot as production-ready.
