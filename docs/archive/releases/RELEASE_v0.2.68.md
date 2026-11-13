# Release v0.2.68 — success-only ReqRing completion

Date: 2026-11-10
Baseline: v0.2.67
Project root: /root/att

## Change

The ReqRing contract states that only successful operations are cached and failed operations must be aborted so a retry can execute again. Before this increment, `finish()` accepted any `ApiCode` and stored it as a replay result. A handler could therefore cache a transient failure and every identical retry would replay that failure rather than re-run.

Changes:
- `finish()` now returns bool.
- It accepts only `A_OK` and a matching live (req_id, ticket) reservation.
- Failure codes, stale tickets and zero tickets return false without changing the pending reservation.
- The caller must explicitly `abort(req_id, ticket)` after a failed or uncertain operation before retrying.
- Successful result replay and stale-ticket protections remain intact.

No persistent event format, product policy, or delegated decision changed. There are no production ReqRing call sites in this repository; external consumers must check the bool result and abort unsuccessful reservations.

## Verification

Test-first regression: added before implementation. The pre-change test command:

```sh
pio test -e native -f 'test_api'
```

failed to compile because `finish()` returned void; 0 tests executed, exit 1. This is interface-stage red evidence, not a runtime failure.

Final command:

```sh
pio test -e native -f 'test_api'
```

Result: **41/41 passed**, exit 0. The test verifies a failed `finish(A_INVALID)` is rejected, the reservation remains pending, explicit abort permits a retry, and the later `A_OK` is replayed. Static call-site audit found ReqRing is used only in `test/test_api` and has no production caller under `lib/`.

Inventory: **611 C++ registrations across 35 suites**; the other **570 registrations were not run**.

## Critical limits

- ReqRing is RAM-only and bounded. It does not make storage appends transactional or durable.
- A false/uncertain finish leaves the matching reservation pending; the caller must abort only after reconciling whether the side effect committed.
- Expiry still does not cancel the original handler. If timed-out work continues while a retry begins, duplicate side effects remain possible.
- Finished entries may be evicted, so old request IDs can run again.
- U112, C3 and C13 remain open. No production HTTP/API caller, ESP32 target build, final ELF, SF2 test, flash or physical-device verification was performed.
- HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
