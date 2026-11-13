# ATT-U61-BACKDATED-ROSTER-01 — SHRU regression handoff

**Status:** READY FOR INDEPENDENT REVIEW  
**Baseline:** v0.2.95 canonical shared tree at `/root/prototype/att`  
**Workspace:** Canonical shared tree; RDC read/write and read-back succeeded.  
**Integrity:** Repository manifest remains stale and is not a working-tree integrity attestation.  
**Scope:** Test-only changes to the two assigned test files. No source, specification, authority, coordination, working-memory or build-configuration files changed.

## Changes

### `test/test_roster/test_main.cpp`

Added three registrations (suite now **21 definitions / 21 registrations**):

1. `test_planners_reject_backdated_changes_per_learner_and_preserve_same_day_noops`
   - Proves a later event for one learner does not block a chronologically valid rename for another.
   - Verifies earlier-day rename and deactivation return `RE_BACKDATED` with zero output events.
   - Verifies earlier-day activation after a later deactivation returns `RE_BACKDATED` with zero output events.
   - Preserves same-day changed-name rename and same-day opposite lifecycle admission, plus no-op behavior.
2. `test_planners_reject_pre_epoch_days_with_zero_output`
   - Verifies pre-2020 timestamps return `RE_BAD_DAY` for SET, DEACTIVATE and ACTIVATE paths, with zero output events.
   - Confirms a true same-name no-op remains `RE_NOOP` even when the unused epoch is before the supported date range.
3. `test_raw_backdated_rename_deactivate_and_activate_fault_live_and_replay`
   - Constructs raw log records that bypass the planners for all three action types.
   - Confirms the live observer marks history untrusted without applying those backdated mutations.
   - Reopens the same log through a fresh `LogStore` / `RosterStore` and confirms replay also marks history untrusted, with unchanged name/lifecycle state.

A small local `makeRawSetEvents()` fixture helper creates a valid five-record SET group so this test exercises the raw-record path rather than the planner.

### `test/test_api/test_main.cpp`

Added one registration (suite now **50 definitions / 50 registrations**):

- `test_roster_backdated_and_invalid_day_error_mapping`
- Verifies earlier-day rename/deactivation/activation map to `A_CONFLICT` + `R_BACKDATED`, with zero output events.
- Verifies pre-epoch input maps to `A_INVALID` + `R_BAD_ROSTER_DAY`, with zero output events.
- Preserves API no-op mapping as `A_OK` with zero output events.

E27 is preserved: same-day opposite lifecycle actions remain operationally admissible in the roster store; the existing monthly projection restriction is unchanged.

## Registration audit

| Suite | Definitions | RUN_TEST registrations | Duplicate / missing / unregistered |
|---|---:|---:|---|
| `test_roster` | 21 | 21 | None |
| `test_api` | 50 | 50 | None |

These are source inventory counts, not passing test results.

## Exact files and checksums

| Path | Before SHA-256 | After SHA-256 |
|---|---|---|
| `test/test_roster/test_main.cpp` | `4c774ce2bd5ef3d11042034e65d6076da7bb213a9f1c4a88b0d7e05d6afd5aab` | `6191c61afb2b032ac4f35c054c1a22571d35ff3f9fdc7abfc6cacff0a8514882` |
| `test/test_api/test_main.cpp` | `5d19b7a40c63ae9577bd9d00f48796e4591cf1be811940f97969d3f833e64ffc` | `88f69031d5991f049fc0b14d4d37dc3ad5c4cc4ae03410d5af6004504ea57f3f` |

Pre-edit copies:
- `comms/staging/SHRU/ATT-U61-BACKDATED-ROSTER-01/before/test_roster_test_main.cpp`
- `comms/staging/SHRU/ATT-U61-BACKDATED-ROSTER-01/before/test_api_test_main.cpp`

Unified diff:
- `comms/staging/SHRU/ATT-U61-BACKDATED-ROSTER-01/patch.diff`
- SHA-256: `3007a85d7f1582cb245c4e8c4bcb0f84382fa7a03b5a0f1eb00fa05953f442de`

Diff contains only the two assigned test paths. Each changed file was read back and compared against its pre-edit copy.

## Verification limits

- Source read-back / diff inspection / registration audit: **DONE**.
- Tests, compilers, builds, PlatformIO, flashing and hardware: **NOT RUN / NOT AUTHORIZED**.
- The assignment did not authorize source changes; none were made.
- Independent PISHDA review is still required before acceptance/integration.
