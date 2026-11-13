# Audit v0.2.66 — checked API input/output boundaries

Date: 2026-11-09
Baseline: v0.2.65
Project root: /root/att

## Baseline identity

- README marker before edits: v0.2.65 (2026-11-09).
- Git metadata is absent in this archive-based tree.
- v0.2.65 archive checksum and source manifest were verified before editing.
- Protected file hashes before and after this increment:
  - HANDOFF_FINAL.md: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d
  - docs/DECISIONS.md: d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44
- Recorded v0.2.65 baseline hashes:
  - lib/core/src/api_logic.h: 2a7813bd038f5a1cc7c4b095af4cf0ac6ec1ec11aae06620b2db8d7fb01a45d3
  - lib/core/src/api_logic.cpp: 60e10708f3d25552f9c0eba8b307f889290fecbc595e10af1c75cce6fa6fa80d
  - test/test_api/test_main.cpp: 0d969367d8fe7ab3c5ea610941fb004774f5cf051e83c6d092159ddcd188e4f5

## Findings

1. reqFingerprint() accepted a pointer/length pair and dereferenced the pointer for nonzero lengths. Its uint32 return type provided no distinct invalid-input channel. Using a magic digest for invalid input would permit a valid request to collide with the invalid marker.
2. buildMark() and buildNoClass() wrote to Event* out on valid requests without checking null.
3. RosterStore::planSet(), planDeactivate() and planActivate() wrote to Event* out without checking null.
4. RosterStore::nameOf() copied into its caller-provided destination without checking null.

These are core API availability/robustness defects. No production HTTP route exists in this tree, so this audit does not claim remote exploitability.

## Patch

Files changed:
- lib/core/src/api_logic.h
- lib/core/src/api_logic.cpp
- lib/core/src/roster_store.h
- lib/core/src/roster_store.cpp
- test/test_api/test_main.cpp
- test/test_roster/test_main.cpp
- docs/SPECS/api_logic.md
- docs/SPECS/roster_store.md
- README.md, RELEASE.md, RELEASE_v0.2.66.md
- docs/CONTEXT.md, docs/CHANGELOG.md, docs/TESTS.md, docs/CRITICAL.md
- docs/AUDIT_v0.2.66.md, tools/verify_v0.2.66.sh
- MANIFEST.sha256 regenerated for release.

Behavior:
- reqFingerprint() now returns bool and only writes outFingerprint after validating the pointer/length pair. Null with n > 0 returns false; null with n == 0 is accepted.
- Both API Event builders reject null output and leave cnt = 0.
- All RosterStore plan functions reject null output with appended RE_BAD_OUTPUT, leaving cnt = 0.
- API wrappers map RE_BAD_OUTPUT to A_INVALID/R_BAD_FIELD.
- nameOf() returns zero for null output or unknown ID.

RE_BAD_OUTPUT was appended after all existing RosterErr values, preserving their numeric values. Existing persistent formats are unchanged.

## Test evidence

Tests were authored before implementation.
- Initial test_api compile attempt failed because the new checked reqFingerprint() signature had not yet been declared.
- Initial test_roster compile attempt failed because RE_BAD_OUTPUT had not yet been appended to the enum.
- These were expected interface-stage compile failures; they are not runtime red-phase evidence. The source inspection showed the unguarded pointer dereferences, but this release does not claim a pre-fix runtime crash reproduction.

Focused final batch:
- pio test -e native -f 'test_api' — 39/39 passed, exit 0.
- pio test -e native -f 'test_roster' — 15/15 passed, exit 0.
- pio test -e native -f 'test_roster_history' -f 'test_month_roster' — 23/23 passed, exit 0.
- Combined exact final verifier: ./tools/verify_v0.2.66.sh targeted — 77/77 passed across 4 suites, exit 0.

The dependent suites were selected because roster planning methods are used by roster-history replay helpers, and nameOf() is called by month-roster projection. This is a targeted batch, not a full regression run.

## Critical review / residuals

- No pointer guard can validate dangling pointers, undersized buffers, or arbitrary invalid non-null addresses. A valid-looking pointer may still refer to insufficient memory.
- nameOf()'s existing size_t result uses zero for both unknown ID and null output; this is safe and documented but does not distinguish those cases.
- reqFingerprint() is a source-level API signature change. Repository search found no production caller; only the test call sites existed. Any out-of-tree consumer must migrate to the boolean/output-reference contract.
- Null event outputs are now explicit errors, but non-null buffer capacity remains a caller precondition. The builder signatures still cannot verify the actual capacity behind Event*.
- No production HTTP/API adapter, target build, ELF inspection, flash or physical-device verification exists. C3/C6 remain blocking.
- HANDOFF_FINAL.md and docs/DECISIONS.md were unchanged.
