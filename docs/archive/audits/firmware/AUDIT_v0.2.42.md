# Specialized audit — v0.2.42

Date: 2026-11-08  
Base: complete `att_v0.2.41_2026-11-08.tar.gz` archive  
Base SHA-256: `cd7dae882565a1b534c70477de12fd49f4f98a68a9d93ef26a3b7a3bd41daf4f`

## Finding and root cause

The user's v0.2.41 PlatformIO run collected 29 native suites. The user reports that all suites except `test_esp32_wifi_ap` passed. The supplied excerpt shows that the AP test object compiled and the static `hal_esp32` library linked into the test executable, but final executable linking stopped on:

```text
undefined reference to `esp_fill_random'
```

The dependency is real: `Esp32WifiAp` owns an `Esp32SecureRandom&`, and `esp32_secure_random.cpp` is linked into `hal_esp32`. `esp_random.h` was available as a declaration through the host stub include path, but the AP suite's test executable did not define the C-linkage symbol. The definition in `test_esp32_secure_random/test_main.cpp` cannot satisfy another test because PlatformIO builds every suite as a separate executable.

This failure occurred at link time. None of the AP suite's 20 test functions ran in the user's failed v0.2.41 attempt. It does not indicate that an ESP32 API is missing on target or that Wi-Fi startup failed on hardware.

## Patch

Only the AP suite host test was changed:

- Added an explicit `#include <esp_random.h>` so the fake is type-checked against the suite's declared API.
- Added a deterministic `extern "C" esp_fill_random(void*, size_t)` implementation local to `test/test_esp32_wifi_ap/test_main.cpp`.
- Added an invocation counter and deterministic byte sequence for assertions.
- Extended the existing AP/RNG integration test to prove: (1) RNG fill is refused before AP startup; (2) the AP_START event opens the gate and permits a fill; (3) the fake receives and fills the requested buffer; and (4) AP stop closes the gate before a subsequent fill attempt.

No production source, production adapter, public API, signed decision, delegated decision or test registration count changed. The test inventory remains **487 `RUN_TEST` registrations across 29 suites**; `test_esp32_wifi_ap` remains 20 registrations.

## Verification performed here

| Check | Result | Evidence limit |
|---|---:|---|
| AP suite host compile/link and run | 20 tests, 267 assertions, 0 failures | g++ + temporary Unity-compatible stand-in; ASan/UBSan enabled |
| SF2 Node suite | 257/257 passed | Node.js host test only |
| JavaScript syntax | 25/25 `.mjs` files passed `node --check` | Syntax only |
| C++ registration inventory | 487 across 29 suite directories | Source scan, not an execution count |
| Production source delta | 0 files | Only test and documentation files changed |
| PlatformIO in packaging environment | Not run | `pio`/`platformio` is not installed here |

The AP stand-in run used `ASAN_OPTIONS=detect_leaks=0`. The existing `test_stop_failure_keeps_rng_blocked_and_does_not_destroy_live_resources` deliberately retains an event group while the simulated driver/handler may still reference it; that retained callback context is expected until reboot. Leak detection was disabled for this focused run only. AddressSanitizer and UndefinedBehaviorSanitizer remained enabled.

## User-reported PlatformIO result to carry forward

The user reports that all 28 non-AP suites passed in the v0.2.41 run. The pasted excerpt documents the AP link failure but does not include the final full-suite summary or per-suite counts. Do not label the corrected v0.2.42 archive PlatformIO-verified until the user reruns it.

Required next commands from a clean extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

## Remaining product and hardware blockers

- No ESP32 target build or device test has been performed for the AP/bootstrap adapters.
- `bootloader_support` target linkage, SAR-ADC entropy behavior and its exclusion from concurrent Wi-Fi/ADC/I2S activity remain unverified on the actual ArduinoDroid toolchain.
- No production boot composition, OLED driver/disclosure flow or production AP caller exists.
- E29 production SSID naming and E30 password reset/rotation/recovery remain OPEN.
- C6 remains blocking for production API security; PIN verifier/KDF, HTTP/JSON wiring and protected routes do not exist.
- NVS interruption/power-cut behavior, OLED disclosure ordering, AP first-beacon WPA2/CCMP, two-client cap and 10-hour soak remain unverified.

## Source delta against v0.2.41

Existing files changed:

- `README.md`
- `RELEASE.md`
- `docs/CHANGELOG.md`
- `docs/CONTEXT.md`
- `docs/CRITICAL.md`
- `docs/TESTS.md`
- `docs/UNVERIFIED.md`
- `test/test_esp32_wifi_ap/test_main.cpp`

New files:

- `RELEASE_v0.2.42.md`
- `docs/AUDIT_v0.2.42.md`

No deletions. The source change is strictly test-only. `.git`, `.pio`, `node_modules`, Python caches and generated build outputs are excluded from the release archive.
