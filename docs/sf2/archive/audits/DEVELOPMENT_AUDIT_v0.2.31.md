# Specialized engineering audit — v0.2.31

Date: 2026-11-07  
Scope: C6 PIN-attempt sequencing core  
Base: uploaded v0.2.30 archive

## Result

A hardware-independent `PinAuthCoordinator` was added in a new `lib/core/src/pin_auth.h/.cpp` module. The adapter design and config-boot specification were updated to prevent stale documentation from implying the counter adapter is fully absent or the helper is already wired into boot/HTTP. The concrete adapter remains absent. It centralizes sequencing that previously existed only as caller comments around `PinLockout`:

1. Load the persisted failure count during boot; fail closed if the read fails.
2. Validate the six-digit input shape.
3. Check monotonic lockout timing and increment the in-memory counter.
4. Persist the incremented counter before the secret verifier runs.
5. On mismatch, retain the persisted attempt and apply the existing `PinLockout` delay.
6. On verifier error, latch the coordinator rather than misreporting an ordinary wrong PIN.
7. On match, clear and durably persist the counter before invoking a mandatory success action.
8. On persistence failure, do not proceed to verification/action and latch the coordinator.
9. PIN change is a dedicated method: validate the new PIN and reject weak patterns, then verify the current PIN, persist the cleared attempt counter, and only then call the injected new-PIN writer.
10. Indeterminate action/write state latches the coordinator until reboot/reload.

The new helper does not implement NVS, PBKDF2, token generation, TokenTable, WebServer, JSON, route authorization or firmware composition. It can be bypassed until the production adapter routes every login, setup-code and current-PIN check through it. Therefore **C6 remains OPEN/BLOCKING**.

## Critical design boundaries

- `IPinAttemptStore::persistFailureCount()` must return success only after durable commit and read-back. A bare successful enqueue or write call does not satisfy the interface contract.
- `IPinVerifier` distinguishes `PV_MISMATCH` from `PV_ERROR`; cryptographic implementation failure must fail closed.
- `IAdminPinWriter` must generate salt/hash using the signed credential contract, commit and read back; it must never persist the raw PIN. No KDF parameter or PBKDF2 performance claim was invented.
- The success action runs only after the durable failure-counter reset. An indeterminate action causes a coordinator fault; the eventual adapter must reject all protected routes while faulted and ensure no partial session/privilege remains usable.
- The coordinator is synchronous and serialized-main-loop only. A multi-task adapter needs a mutex covering the entire authentication/action transaction.
- The coordinator is one long-lived object per chosen lockout-counter domain. E12 remains DELEGATED; this patch does not silently choose which routes share the same counter.
- No storage schema, Config layout, NVS namespace/key, backup content, signed handoff or user decision was changed.

## Regression registrations

Thirteen new `RUN_TEST` registrations were added to the existing `test_api` suite. They cover boot read failure, persist-before-verify order, counter reset before action, mismatch persistence, lockout after the fifth failure and reboot restoration, counter-write failures, verifier error, malformed input, indeterminate action, current-PIN enforcement, weak PIN rejection, old-PIN mismatch and ambiguous new-PIN write. The ordering tests also inspect the fake store at the exact verifier/action/writer call boundary: the verifier must observe the incremented count, and the privileged action/new-PIN writer must observe the durable zero reset. This proves the helper passes the expected values to the injected store in the host fake; it does not prove a future NVS adapter actually commits or reads back durably.

They are **authored but unrun**. They are not passing tests.

## Static checks

- `g++ -std=c++11 -Wall -Wextra -Werror -Ilib/core/src -fsyntax-only lib/core/src/pin_auth.cpp`: passed.
- `g++ -std=c++11 -Wall -Wextra -Werror -Ilib/core/src -I/tmp/unity_stub -fsyntax-only test/test_api/test_main.cpp`: passed with a temporary Unity stub that only evaluates syntax and expressions. It does not execute any Unity assertion.
- The attempted direct test-source syntax command initially failed because the archive contains no Unity header; the temporary inert stub was used only for syntax validation, not committed.
- No functional tests, PlatformIO, C++ link, ESP32 target build, HTTP/AP test, or hardware validation was run.

## Test accounting

| Metric | v0.2.31 |
|---|---:|
| C++ registration count | 389 across 23 suites |
| User-reported passing baseline | 318/318 on v0.2.17 only |
| Later C++ registrations | 71, unrun |
| SF2 Node registrations | 256, all unrun |
| Test registrations added | 13 |

## Source-tree review

- Relative to the uploaded v0.2.30 archive, 17 paths differ: 13 existing documentation files, one existing `test_api` source file, two new production source files, and this new audit file.
- Existing production modules, including `api_logic.*`, were left unchanged; `pin_auth.*` is additive.
- No existing test module was deleted or regenerated; 13 cases were added to `test_api`. The test fake was strengthened to assert the persisted counter value at the verifier and success-action/writer boundaries.
- No PII, legal learner identity or evidence fields were added to storage or any API.
- O8 (backup = config + roster + log), R11 threat boundary, R12, R13-R17, E12 and E27 were not changed.
- `.git/`, build outputs and package caches are excluded from the archive.

## Archive verification

- Complete archive: `att_v0.2.31_2026-11-07.tar.gz`.
- Manifest entries: 202; all 202 passed independent `sha256sum -c` after extracting the archive into a clean directory.
- Extracted files: 203, including `MANIFEST.sha256`.
- No `.git/`, `.pio/`, `node_modules/`, `__pycache__/` or `.cache/` paths are included.

## Remaining blockers

C6 requires a real durable attempt store, pinned PBKDF2 implementation and parameter policy, token RNG and TokenTable integration, HTTP route wiring, adapter-level tests and status redaction. U79 still requires AP passphrase provisioning/strength evidence. C13/C26-C29/C31/O8 continue to block metadata persistence and authoritative SF2. No production firmware or dashboard exists.

**Audit conclusion:** useful C6 core progress, but not production security enforcement. Do not expose PII or mark the API secure until the adapter and device path are implemented and tested.
