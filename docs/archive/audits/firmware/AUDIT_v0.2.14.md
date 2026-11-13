# Specialized source audit — v0.2.14

Date: 2026-11-06
Scope: restore replay audit, validator/staging boundary, NVS fake SDK header, release metadata.
Execution: **static inspection only**. No compiler, PlatformIO test, firmware build or device test was run, as requested.

## Findings corrected

### 1. NVS fake-header constant drift

The v0.2.13 test stub assigned `ESP_ERR_NVS_NOT_INITIALIZED` the value `0x1105`. In ESP-IDF release v5.5, the correct value is `0x1101`; `0x1105` is `ESP_ERR_NVS_NOT_ENOUGH_SPACE`. The stub now has the correct values, and `test_nvs_config` contains six compile-time `static_assert`s for the numeric constants used by the fake API. The production backend uses symbolic SDK names; this correction does not alter its code. The values were checked against Espressif's release-v5.5 error-code reference: <https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/api-reference/error-codes.html>.

### 2. Staging mutation between validation and replay

Before this release, `RestoreReplayAudit` checked staged length, then relied on the record CRC/sequence checks in `LogStore::open()`. A changed record with a freshly recomputed record CRC could evade that second layer even though it differed from the bytes accepted by `RestoreValidator`.

`RestoreValidator` now accumulates CRC32 over the exact 16-byte records it accepts. `RestoreReplayAudit` recomputes that CRC over the staged bytes before opening the staged log and rejects read failure or mismatch. This is an accidental-mutation cross-check only. CRC32 is not a cryptographic MAC and does not prevent concurrent mutation after the check. The future restore caller must exclusively own/freeze staging from validation through replay and commit.

### 3. Event commit-group shape and arithmetic

The new `RestoreGroupAudit` was checked against the current event writers in `roster_store.cpp`, `closure_engine.cpp`, `log_store.cpp` and `clock_policy.cpp`. It accepts the actual current group forms:

- ROSTER SET header plus four payload records; ACTIVATE/DEACTIVATE as a single record.
- CLOSE header plus zero to five payload words. Payload population must equal the header's expected-student count; unused bits above `MAX_ID` must be zero.
- GAP chunks from one cause, with positive minute counts and exact continuity between chunks in each atomic group; the final GAP group may carry the two-record CLOCK_SET pair.
- CLOCK_SET as a two-record source-matched pair.
- Other current non-payload event types as single-record groups. EV_PAYLOAD cannot stand alone.

The audit rejects discontinuous GAP chunks within an atomic group and GAP end-time arithmetic above `UINT32_MAX`, because `LogIndex` stores its end as a 32-bit epoch. It does not attempt to validate every field of every attendance/business event or to reinterpret historical policy decisions.

### 4. Observer and closure safety

`RestoreReplayAudit` constructs a fresh `LogStore`, `RosterStore`, and `ClosureEngine`; the engine attaches its `LogIndex` before the staged replay. The roster and group observers are attached before `open()`. The audit checks record count/last sequence, corruption, torn bytes, dropped groups, roster malformed/overflow state, and index overflow.

It deliberately does **not** call `ClosureEngine::step()`. That routine can append synthesized CLOSE events, which would alter the restored log. This increment rebuilds the index; it does not synthesize closures.

`LogStore::open()` may truncate a damaged staged tail before returning its `OpenInfo`. The audit rejects any torn or dropped tail and is one-shot; callers must discard a failed stage rather than reuse it.

### 5. Memory and lifecycle

`RosterStore` and `ClosureEngine` contain fixed-size tables. `RestoreReplayAudit` must be allocated statically or from the heap, not as a local variable on a small task stack. It must be constructed only after `RestoreValidator.ok()` because it snapshots the validated config in its constructor. A construction-order guard rejects early construction. It caches its first result and cannot be rerun.

## Remaining blockers / limits

- C2 remains blocking: there is no restore transaction coordinator, persistent recovery journal, atomic storage swap, post-swap reopen, reboot recovery, or acknowledgment protocol.
- C13 remains unresolved: no LittleFS adapter or power-cut behavior has been proven. This release intentionally does not implement a rename/copy/truncate swap based on an assumption of atomicity.
- The stage is not locked by this component. Exclusive ownership is a caller contract, not an enforced mutex.
- The CRC32 comparison is not cryptographic and does not prove a frozen stage during the subsequent second read by `LogStore::open()`.
- Semantic replay covers roster reconstruction, current commit-group shape and closure index capacity. It does not prove every mark/no-class/session/business rule, validate hardware mappings, or synthesize missing closes.
- The NVS stub assertions are authored only; without compilation they are not confirmed to compile.
- No tests or compiler were run for v0.2.14. The current 295-case suite was not run as a whole; v0.2.10–v0.2.14 additions remain unexecuted. Historical v0.2.8 user-reported PlatformIO and v0.2.9 local stand-in results remain recorded in `docs/TESTS.md`.

## Audit disposition

The identified source/documentation defects were corrected. This archive is suitable as the next **development snapshot**, not as a verified restore implementation or production firmware release. Do not enable restore or attendance in a deployed device from this source-only audit.

## Post-audit verification addendum

On 2026-11-06, after this static audit snapshot was packaged, the user reported **295/295 native tests passed** on v0.2.14. This later user-reported run supersedes the audit-time statement that the 295-case suite had not yet been executed. It does not establish an ESP32 target build, NVS power-loss behavior, restore swap/recovery, or physical sensor compatibility.
