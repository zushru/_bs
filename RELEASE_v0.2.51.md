# Release v0.2.51 — AS608 attendance-pipeline integration tests

**Date:** 2026-11-09  
**Base:** complete v0.2.50 working tree  
**Status:** targeted native verification passed; no ESP32 target rebuild, flash or hardware test.

## Changes

- Added test/test_as608_pipeline, a dedicated PlatformIO native suite for the path from sensor match through As608Runtime, injected physical-page resolver, roster/admission checks and durable EV_SCAN append.
- The fixture uses the real As608Runtime composition root and mock storage/UART only. It deliberately injects a test-only page mapping; it does not decide or endorse E18.
- Six tests cover a successful Present event, Late event, duplicate suppression, unresolved page mapping, log append failure, and no-finger handling/lease release.
- Added tools/verify_v0.2.51.sh, which runs seven focused suites and the 11 ELF-inspector fixtures. It never uploads or flashes.
- Restored executable mode on the inspector scripts because the working-tree extraction had made them non-executable; this had prevented the fixture harness from launching its inspector.
- No production runtime source, product behavior, hardware mapping, or decision ledger changed.

## Verification performed

| Suite | Result |
|---|---:|
| test_nvs_startup_guard_policy | 9/9 |
| test_hardware_probe | 4/4 |
| test_as608 | 15/15 |
| test_as608_gate | 26/26 |
| test_as608_pipeline | 6/6 |
| test_attendance_admission | 12/12 |
| test_enroll | 36/36 |
| **Total** | **108/108** |

- Inspector fixtures: 11/11 passed (3 accepted, 8 rejected).
- Verifier: ./tools/verify_v0.2.51.sh targeted exited 0 with TARGETED_V0_2_51=PASS.
- Static inventory: 539 C++ registrations across 34 suites. The other 431 registrations were not run.
- SF2 source unchanged; its 257/257 Node result and 25 syntax checks remain historical evidence from the prior release, not rerun here.
- ESP32 smoke/probe builds and exact-ELF routing inspections remain the v0.2.50 results. They were not rerun because this increment changes only native tests, scripts and documentation.

## Critical limitations

1. C3 remains blocking. This suite validates the hardware-independent scan-to-log contract only. There is still no production scheduler, complete firmware entry point, production event-pipeline composition, HTTP route or admin UI.
2. E18 remains DELEGATED and unresolved. The successful-path mapping in this suite is a fake injected only to exercise the pipeline. It is not the production student-ID/physical-page mapping. Do not enable production enrollment/attendance until E18 is explicitly resolved and Job 11 evidence is reviewed.
3. No real AS608 sensor, UART timing, template page, RTC, storage driver, power-cut, AP or display behavior was tested.
4. C34 remains blocking. The NVS wrapper remains a narrow guard and is not a production startup/recovery solution.
5. No device was flashed. Never flash the inert adapter-link smoke image.

## Install

Install only by replacing the complete project tree with the full v0.2.51 archive. Do not merge individual files into an older tree.
