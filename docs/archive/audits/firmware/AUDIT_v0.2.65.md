# Audit v0.2.65 — token API null-pointer crash

Date: 2026-11-09
Baseline: v0.2.64
Project root: /root/att

## Baseline identity

- README marker: v0.2.64 (2026-11-09).
- Git metadata is absent in this archive-based tree.
- v0.2.64 release archive checksum and source manifest were verified before edits.
- Protected file hashes before and after the increment:
  - HANDOFF_FINAL.md: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d
  - docs/DECISIONS.md: d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44
- Baseline hashes before editing:
  - lib/core/src/api_logic.h: 051bd1486b2054b8549b76379c714e916a4a860324d123929fe5503dbefd878d
  - lib/core/src/api_logic.cpp: 7a67c0e735f89c7ef00b83d2751ba3147e41579b61bd06b897b1bc5e48a68dca
  - test/test_api/test_main.cpp: 186ea98d90a225a9d3dfdd859ffe3ff4ca107236ebacf78cefdfe65a6d804561

## Finding and impact

The API deliberately handled null in TokenTable::canIssue(), but did not do so in its other pointer-taking operations. A null check token caused a direct read through tok[k]. A null revoke token reached ctEqual() with a null input if an occupied session slot existed. Direct ctEqual() calls with null inputs and nonzero lengths also dereferenced them.

This is a process-crash/availability defect in the session-token core, not a demonstrated remote exploit: no production HTTP route exists, and the exact caller input exposure is not implemented. The core API nevertheless must not crash on an invalid pointer argument.

## Patch details

Files changed:
- lib/core/src/api_logic.h
- lib/core/src/api_logic.cpp
- test/test_api/test_main.cpp
- docs/SPECS/api_logic.md
- README.md, RELEASE.md, RELEASE_v0.2.65.md
- docs/CONTEXT.md, docs/CHANGELOG.md, docs/TESTS.md, docs/CRITICAL.md
- docs/AUDIT_v0.2.65.md, tools/verify_v0.2.65.sh
- MANIFEST.sha256 regenerated for the release.

Behavior:
- TokenTable::check(nullptr, nowS) returns false without touching session slots or timers.
- TokenTable::revoke(nullptr) returns without touching session slots.
- ctEqual(nullptr, x, n) and ctEqual(x, nullptr, n) return false when n > 0.
- ctEqual(a, b, 0) returns true, including when either or both pointers are null.
- For valid non-null buffers and n > 0, the original accumulator comparison loop remains unchanged.

## Test-first and final evidence

- Added one registered regression, placed last in test_api so the old test log retained evidence for existing cases.
- Red phase: pio test -e native -f 'test_api' reached 37/38 successful registrations, then exited 1 after SIGSEGV in the new null check.
- Green phase: pio test -e native -f 'test_api' -f 'test_pin_auth_session' — 49/49 passed, exit 0. Breakdown: test_api 38/38; test_pin_auth_session 11/11; aggregate suite time 22.267 seconds.
- The stronger regression also verifies no LRU mutation: after two sessions are active and one is refreshed, null check/revoke occur, then a third issue must evict the original least-recently-used session.
- Inventory is 607 registrations across 35 suites; 558 were not run.

## Critical review and residuals

- A null guard cannot validate dangling pointers, undersized buffers, or invalid non-null memory. Callers still must provide valid storage for every nonzero comparison length.
- This change does not add request parsing, route-level validation, mutual exclusion, reservations, or a production session owner.
- TokenSessionSuccessAction still relies on serialized TokenTable mutation between preflight and response confirmation. C6 remains blocking; no production route proves this invariant.
- No full native/SF2 tests, target build, ELF inspection, flash, NVS operation, hardware or power-loss test was performed.
- No product, signed or delegated decision was changed. HANDOFF_FINAL.md and docs/DECISIONS.md remained unchanged.
