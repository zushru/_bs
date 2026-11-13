# Audit v0.2.33 — month-roster test-fixture correction

## 1. Inputs and provenance
- Base: complete `att_v0.2.32_2026-11-07.tar.gz` present in the working container.
- Base archive SHA-256: `893b30489592596bdf5d0ad40f269dc40cc2f63c3c7357eb24edcbdf34524b67` (computed directly before extraction).
- New evidence: user-supplied `native-test.log` from the v0.2.32 PlatformIO run.
- Archive was extracted as a complete tree before inspection. No `.git/`, `.pio/`, `node_modules/`, or build cache is included in the release package.

## 2. Test-log result
- 23 native suites attempted; 22 were marked `PASSED`.
- `test_event_group_audit` passed all 19 displayed assertions. This confirms the v0.2.32 direct includes for `MAX_ID` and the `RA_*` declarations resolve the v0.2.31 shared compile blocker.
- `test_month_roster` printed 16 results: 15 passed, one failed at line 126 (`Expected 1 Was 2`). The process then received `SIGHUP`; PlatformIO marked the suite `ERRORED`.
- The terminal summary printed `390 test cases: 1 failed, 388 succeeded`. Source enumeration finds exactly 389 `RUN_TEST` registrations across the 23 suite `test_main.cpp` files. The log does not explain the one-count discrepancy; the suite-level error may be accounted separately by PlatformIO, but this is not proven.
- No other assertion failure is visible in the supplied log.

## 3. Root cause: fixture boundary error, not projection logic
The documented month range is half-open: `[monthStartDay, monthEndExclusive)`. For `[100, 131)`, dates 100 through 130 are included. `MonthRosterProjection::summarizeStudent_()` correctly computes `lastDay = monthEndExclusive_ - 1`, then checks `startDayInclusive <= lastDay && lastDay < endDayExclusive`.

The failing fixture deactivated student 4 on day 131. That creates an interval ending exclusively on day 131, which still includes day 130. Student 4 therefore was enrolled on the final report day. Student 5 was also enrolled. The expected count of one was incorrect; the observed value of two matched both the C++ implementation and the written projection contract. The browser implementation uses the same final-day calculation.

## 4. Changes in v0.2.33
- Corrected the first learner's deactivation effective day to 130, so the learner is not enrolled on final included day 130 while still overlapping the month.
- Added a third C++ learner whose interval ends at day 131 to preserve the boundary case that must count as enrolled on day 130. The C++ test now expects three month-overlap learners and two enrolled on the final day.
- Added one browser-side regression covering intervals ending at day 130 and day 131 for the same `[100, 131)` reporting month.
- Cleaned the compiler warnings present in the supplied log: removed unused `argc`/`argv` parameter names; explicitly initialized two otherwise-zero `Event::b` fields to zero; and removed the unused `staging` parameter from the restore test helper and its call sites. These are test-only cleanup edits and preserve behavior.
- No production source module was changed. No signed/delegated decision, stored event format, backup scope, hardware mapping or SF2 reporting policy changed.

## 5. Test inventory
- C++ registrations: 389 across 23 suites, unchanged. User log evidence is 388 passing assertions plus one failing assertion; the month suite was also marked `ERRORED` after the signal.
- SF2 Node registrations: 257, up from 256 by one new boundary regression.
- No C++ compile, PlatformIO test or Node functional test was run for v0.2.33. The source/test edits are therefore unverified until the user's rerun.
- No ESP32 target build or hardware verification was performed.

## 6. Required verification
From a clean full-archive extraction:

```sh
sha256sum -c MANIFEST.sha256
pio test -e native -f test_month_roster -vvv
pio test -e native 2>&1 | tee native-test.log
cd web/sf2
npm test
```

Inspect exact suite exit status, whether `SIGHUP` recurs, total assertions, and any mismatch between PlatformIO's summary and source `RUN_TEST` count. Only actual rerun output may close these unknowns.

## 7. Remaining project blockers
This release only corrects a test fixture. C6 production auth integration, C2 restore swap/recovery, C13 LittleFS power-loss behavior, C1/C17/C18 config history/term boundaries, sensor page-mapping E18, production firmware/API/dashboard, official SF2 workbook validation, metadata persistence, soak and brownout qualification remain unresolved. No product completion claim is made.
