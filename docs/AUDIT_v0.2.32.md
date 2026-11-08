# Audit v0.2.32 — native compile blocker

Date: 2026-11-07

## Inputs checked

- User-supplied `native-test.log`.
- User-supplied `att_v0.2.31_2026-11-07.tar.gz`.
- Archive SHA-256 independently matched the issued value: `3bfa9c022b87bd9273ea2097941062dacca17ae6179727aaa583cac42b1adee7`.
- Read `docs/CONTEXT.md`, `docs/CRITICAL.md`, `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, `docs/UNVERIFIED.md`, `docs/TESTS.md`, `RELEASE.md`, `platformio.ini`, and the relevant event-audit implementation/tests.

## Observed test result

PlatformIO attempted 23 native suites. All 23 ended with `ERRORED` during the build stage; the final summary was `23 test cases: 0 succeeded`. These entries are suites in this PlatformIO output. They did not reach assertion execution, so this run provides no functional pass/fail evidence for any suite.

Every shown fatal error came from `lib/core/src/event_group_audit.cpp`:

- `MAX_ID` undeclared.
- `RA_SET` undeclared.
- `RA_DEACTIVATE` undeclared.
- `RA_ACTIVATE` undeclared.

Source inspection found `MAX_ID` declared in `day_view.h` and the roster action enum declared in `roster_store.h`. `event_group_audit.cpp` previously included neither defining header. The shared library source is compiled in each suite, explaining why the same translation-unit error blocked all suites.

The log also contains `-Wmissing-field-initializers` warnings for two `Event` aggregate initializers in `test_coreb2`; those warnings were not the fatal build error. They were not changed in this patch.

## Correction made

`lib/core/src/event_group_audit.cpp` now includes:

```cpp
#include "day_view.h"
#include "roster_store.h"
```

No algorithm, event structure, test source, registration count, signed rule, delegated decision, hardware pin, backup policy or SF2 policy was changed. The existing `test_event_group_audit` suite has 19 registrations; no new registrations were added.

## Verification limits

- The added includes statically resolve to the header declarations seen in the supplied tree.
- No compiler or PlatformIO run was performed for v0.2.32.
- This correction may reveal additional build errors previously hidden by the first translation-unit failure.
- The 71 C++ registrations added after v0.2.17 remain unrun. The 318/318 historical pass remains attributable only to the user's v0.2.17 report.
- All 256 SF2 Node registrations remain unrun.
- No exact ESP32 build, hardware behavior, power-loss behavior, sensor behavior or device test is established.

## Required verification

From a clean extraction of this complete archive:

```sh
pio test -e native -f test_event_group_audit -vvv
pio test -e native 2>&1 | tee native-test.log
```

A targeted suite pass is not a substitute for the full suite. Do not merge individual files into an older source tree.
