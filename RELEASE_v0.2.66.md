# Release v0.2.66 — checked API input/output boundaries

Date: 2026-11-09
Baseline: v0.2.65
Project root: /root/att

## Change

This increment hardens related pointer/length and caller-output boundaries in the pure core API.

### Request fingerprint

The previous reqFingerprint() returned a bare uint32 digest while dereferencing the supplied byte pointer for n bytes. A null pointer with nonzero length had no safe result channel. Returning a special digest value would be unsafe because every uint32 value is a legitimate possible fingerprint.

The function now returns bool and writes the digest through an output reference only on success:
- Null pointer with nonzero length returns false and leaves the output unchanged.
- Null pointer with zero length remains valid and hashes the empty body header.
- Valid non-null inputs retain the same FNV-1a computation.

The function had no production caller in this repository; existing test call sites were migrated to the checked signature.

### Event output buffers

- buildMark() and buildNoClass() now reject a null Event* with A_INVALID/R_BAD_FIELD and set the output count to zero.
- RosterStore::planSet(), planDeactivate() and planActivate() reject a null Event* with the new RE_BAD_OUTPUT value and set count to zero.
- RE_BAD_OUTPUT is appended to RosterErr; all previous enum numeric values are preserved.
- API wrappers explicitly map RE_BAD_OUTPUT to A_INVALID/R_BAD_FIELD.

RosterStore::nameOf() now returns zero for a null output destination without writing through it. The documented zero result therefore means unknown ID or null destination.

No persistent event encoding, log format, roster policy, signed/delegated decision, or storage behavior changed.

## Verification

Final selected suites:
- test_api — 39 registrations.
- test_roster — 15 registrations.
- test_roster_history — 7 registrations.
- test_month_roster — 16 registrations.

Exact final command: ./tools/verify_v0.2.66.sh targeted — 77/77 passed across 4 suites, exit 0; test_api 39, test_roster 15, test_roster_history 7 and test_month_roster 16. Current inventory: 609 C++ RUN_TEST registrations across 35 native suites. The other 532 registrations were not run.

Test-first reporting: tests were added before implementation. Initial compile attempts exposed the missing checked reqFingerprint() signature and RE_BAD_OUTPUT enum. Those compile errors are interface-stage evidence, not runtime red-phase proof; no pre-fix runtime SIGSEGV is claimed for this increment.

## Not run / limits

- Full native suite and SF2 Node suite.
- ESP32 target build, final ELF inspection, flash and physical-device test.
- Production HTTP/API adapter, dangling/undersized non-null buffer validation, route parsing, target stack/memory behavior or concurrency.
- The null guards can reject null pointers only. They cannot prove that a non-null pointer references a valid buffer of the requested length.

C3 and C6 remain blocking: there is no production firmware/API composition, HTTP request parser or protected route owner. No signed or delegated decision was altered. HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
