# Audit v0.2.68 — success-only ReqRing completion

Date: 2026-11-10
Baseline: v0.2.67
Project root: /root/att

## Baseline identity

- README marker before edit: v0.2.67 (2026-11-10).
- v0.2.67 archive SHA-256 verified before edits: ce3e8ddd151faae3acaf03eb31610332f3e0e31b0ba2e237f3cd9b3f47e83e7c.
- v0.2.67 sidecar passed and all 360 manifest entries verified before edits.
- Git metadata is absent in this archive-based tree.
- Protected file hashes:
  - HANDOFF_FINAL.md: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d
  - docs/DECISIONS.md: d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44

## Finding

The ReqRing header documented that failures must abort and only successful operations should be finished. Implementation did not enforce that rule: `finish(reqId, ticket, code, aux)` cached any ApiCode. A caller that passed A_INVALID/A_CONFLICT could turn a transient failed attempt into a permanent replay result for that req_id/fingerprint until entry eviction.

## Patch

- Changed `finish()` from void to bool.
- It returns true only when code == A_OK and a live entry matches both req_id and ticket.
- Failure codes and stale/zero tickets return false without mutating the entry.
- The test proves a rejected failure result remains RQ_INFLIGHT until explicit abort, after which a new attempt can be reserved, successfully finished and replayed.
- Existing successful callers in the test suite can ignore the bool return, but production integrations must check it.

Files changed:
- lib/core/src/api_logic.h
- lib/core/src/api_logic.cpp
- test/test_api/test_main.cpp
- docs/SPECS/api_logic.md
- README.md
- RELEASE.md
- RELEASE_v0.2.68.md
- docs/CONTEXT.md
- docs/CHANGELOG.md
- docs/TESTS.md
- docs/CRITICAL.md
- docs/UNVERIFIED.md
- docs/AUDIT_v0.2.68.md
- tools/verify_v0.2.68.sh
- MANIFEST.sha256

## Test-first evidence

- Added regression before implementation.
- Initial `pio test -e native -f 'test_api'` failed to compile because the new test required a bool result but finish() returned void. 0 tests executed; exit 1.
- Final `pio test -e native -f 'test_api'` passed **41/41**, exit 0.
- Test covers failed finish rejection, preserved in-flight state, explicit abort, new ticket reservation, successful completion and replay.
- Static search found no production ReqRing call sites under `lib/`; all uses are in test/test_api.

## Critical review

- Returning false for non-A_OK does not itself abort; the caller must explicitly abort after determining the operation did not commit. On an uncertain append, aborting and retrying without reconciliation may still duplicate a side effect. Do not equate the new API contract with transactional durability.
- The ring is volatile, bounded to 16 entries, and finished entries are evictable. It is not durable exactly-once protection.
- Expiry still does not cancel an original handler or reverse its side effects. A caller must serialize/cancel/reconcile old work before allowing retries.
- There is no production HTTP/API composition or caller to validate this contract end-to-end.
- No SF2 tests, ESP32 target build, final ELF, flash or hardware verification.
- HANDOFF_FINAL.md and docs/DECISIONS.md were not changed.
