# Audit v0.2.67 — reservation-ticketed ReqRing completion

Date: 2026-11-10
Baseline: v0.2.66
Project root: /root/att

## Baseline identity

- README marker before edit: v0.2.66 (2026-11-09).
- v0.2.66 archive SHA-256 was verified: eb2c8e1dcdb81f8a8868716b624d11044ef4f276804084251711fd3c092896ed.
- v0.2.66 sidecar check passed and all 357 manifest entries verified before edits.
- Git metadata is absent in this archive-based tree.
- Protected file hashes:
  - HANDOFF_FINAL.md: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d
  - docs/DECISIONS.md: d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44

## Finding

ReqRing::finish() and abort() previously selected a pending entry by req_id alone. expire() could free that entry; begin() could then reserve the same req_id again. A late completion or abort from the old handler could mutate the new reservation, including when req_id and fingerprint were identical.

## Patch

- ReqResult now includes a 64-bit ticket.
- Each RQ_NEW reservation receives a nonzero monotonically incremented ticket. Non-new results leave ticket zero.
- ReqRing::finish() and abort() require req_id plus ticket and ignore zero/stale tickets.
- expire() clears the expired entry's ticket.
- The test exercises expiry, reuse of the exact same req_id and fingerprint, stale finish, stale abort, and successful completion/replay of the current reservation.

Files changed:
- lib/core/src/api_logic.h
- lib/core/src/api_logic.cpp
- test/test_api/test_main.cpp
- docs/SPECS/api_logic.md
- README.md
- RELEASE.md
- RELEASE_v0.2.67.md
- docs/CONTEXT.md
- docs/CHANGELOG.md
- docs/TESTS.md
- docs/UNVERIFIED.md
- docs/CRITICAL.md
- docs/AUDIT_v0.2.67.md
- tools/verify_v0.2.67.sh
- MANIFEST.sha256

## Test-first evidence

- Added the regression before implementation.
- Initial `pio test -e native -f 'test_api'` failed to compile because ReqResult.ticket and the ticketed finish/abort interface were not yet present. It produced 0 executed tests, exit 1.
- After implementation, `pio test -e native -f 'test_api'` passed **40/40**, exit 0.
- After strengthening the regression to reuse the exact same fingerprint as well as req_id, the final release verifier reran the suite successfully: 40/40, exit 0.

## Critical review

- A ticket prevents an old handler from mutating a newer ReqRing reservation. It does not cancel that old handler or prove its side effects did not occur. A caller that expires a still-running handler and permits a retry can still duplicate side effects. The production caller must own cancellation/serialization/reconciliation.
- ReqRing is volatile, 16 entries, and finished entries are evictable. It cannot provide durable exactly-once behavior. U112 remains unresolved.
- 64-bit ticket wrap is theoretically possible after 2^64 reservations; this is not a persistent or cryptographic identity.
- No production ReqRing caller exists in this source tree. External callers must retain the ticket returned by RQ_NEW and pass it to finish/abort.
- No production HTTP adapter, target build, final ELF, SF2 test, flash or physical-device verification was performed.
- HANDOFF_FINAL.md and docs/DECISIONS.md were not changed.
