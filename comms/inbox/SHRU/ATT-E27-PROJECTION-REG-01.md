# ATT-E27-PROJECTION-REG-01 — same-day ACTIVATE then DEACTIVATE projection regression

- Author: SHRU
- Status: COMPLETE
- Baseline: v0.2.95 development snapshot; manifest is stale
- Workspace: CANONICAL_SHARED_TREE
- Inspected: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md; E27 in docs/DECISIONS.md; lib/core/src/month_roster_projection.h/.cpp; lib/core/src/roster_snapshot.h/.cpp; test/test_month_roster/test_main.cpp.
- Not inspected: unrelated roster/event implementation and runtime hardware; no execution was authorized.
- Path discrepancy: assigned read paths roster_transition_audit.h/.cpp do not exist in this baseline. The RosterTransitionAudit type and implementation are in roster_snapshot.h/.cpp; these actual paths were read instead.

## Findings and change

- [INFO] Added test_month_projection_allows_operational_same_day_activate_then_deactivate_but_fails_closed().
- Sequence: initial SET on local day 100, DEACTIVATE on day 110, then ACTIVATE and DEACTIVATE on day 115. Each operational log append must return success; the final sequence number must advance.
- The authoritative monthly projection is then expected to fail closed with MRP_REPLAY_FAILED / RPR_TRANSITION_INVALID and the exact RTA_DEACTIVATE_NOT_FORWARD finding for student 8, source sequence, effective day 115 and previous day 115.
- This covers ACTIVATE-then-DEACTIVATE on the same local day after an earlier deactivation; it does not alter admission or projection policy.
- The new test is registered exactly once. test_month_roster now has 18 source registrations; this is not a test pass.
- Changed project path: test/test_month_roster/test_main.cpp only.
- Pre-edit copy: comms/staging/SHRU/ATT-E27-PROJECTION-REG-01/before/test_main.cpp
- Unified diff: comms/staging/SHRU/ATT-E27-PROJECTION-REG-01/patch.diff

## Verification

- Full changed-file read-back completed.
- Confirmed one test definition and one RUN_TEST registration; saved diff matches regenerated diff; added lines have no trailing whitespace.
- SHA-256 before: c2ccd0fa702b01adec3bd40a7ce8c9d6576a69287e146e1509c619e14d6c4803
- SHA-256 after: 6f2037b60ad7a1538727619c8d47169b4f10bfe0e71a415e9bebd9bb594bc266
- Tests, compiler, build, PlatformIO and hardware: NOT RUN, as assigned.
