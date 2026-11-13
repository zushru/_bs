# Release v0.2.67 — reservation-ticketed request completion

Date: 2026-11-10
Baseline: v0.2.66
Project root: /root/att

## Change

The in-memory `ReqRing` allowed a stale handler to mutate a newer reservation. `expire()` frees an old pending entry; a later request can reuse the same req_id (even with the same fingerprint). The old `finish(reqId,...)` or `abort(reqId)` then matched only by req_id and could incorrectly complete or clear the newer operation.

Each new `begin()` reservation now receives a nonzero 64-bit ticket:
- `ReqResult.ticket` is populated only for `RQ_NEW`; replay, mismatch, inflight and invalid results retain ticket zero.
- `finish(reqId, ticket, ...)` and `abort(reqId, ticket)` mutate only a currently pending entry matching both values.
- Expiry clears the stored ticket. Reuse of the same req_id and fingerprint receives a different ticket.
- Stale or zero-ticket completion/abort calls are ignored.

No event format, persistent storage format, attendance policy or product decision changed. No production caller of ReqRing exists in this tree; out-of-tree consumers must migrate to the ticketed API.

## Verification

Test-first regression: the new test was added before implementation. The pre-change `pio test -e native -f 'test_api'` failed at compile time because ReqResult had no ticket and the ticketed methods did not exist. This is interface-stage red evidence, not a runtime reproduction.

Final command:
```sh
pio test -e native -f 'test_api'
```
Result: **40/40 passed**, exit 0. This includes the new stale-finish/stale-abort regression and all 39 previously registered API cases. Inventory: **610 C++ RUN_TEST registrations across 35 suites**; the other **570 registrations were not run**.

## Critical limits

- Expiry is not cancellation. It does not stop an old handler or undo a side effect already performed. The caller must ensure expired work is no longer active, or serialize/reconcile it before retry; otherwise duplicate side effects remain possible.
- ReqRing is RAM-only and bounded. Finished entries can be evicted; it is not durable exactly-once protection. U112 remains unresolved.
- No production HTTP/API caller exists. No final ESP32 target build, ELF inspection, SF2 tests, flash or physical-device verification was performed.
- HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
