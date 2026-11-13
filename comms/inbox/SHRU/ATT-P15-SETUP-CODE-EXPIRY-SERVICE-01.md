# ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01 — SHRU implementation handoff

**Status:** READY FOR INDEPENDENT REVIEW  
**Baseline:** v0.2.95 canonical shared tree at `/root/prototype/att`  
**Workspace:** Canonical shared tree; RDC read/write and read-back succeeded.  
**Integrity:** Repository manifest remains stale and is not a working-tree integrity attestation.  
**Authority:** Signed E33. The 600-second lifetime, six ASCII digit format, single-use rule, RF-gated RNG lifecycle and route boundaries are unchanged.

## Implementation

Added public `Esp32RamSetupCodeVerifier::serviceExpiry()` in the assigned HAL verifier.

Contract implemented:

- A faulted verifier returns `false`.
- An inactive verifier is a healthy, idempotent no-op; it does not read the clock, touch the OLED, or request entropy.
- An active code before 600,000,000 microseconds remains active.
- At elapsed time **greater than or equal to** 600,000,000 microseconds, the method marks the code inactive, clears its issue timestamp and securely wipes the owned code buffer before requesting exactly one blank OLED transfer.
- The service never rotates or generates a replacement code.
- Clock read failure, monotonic regression, loss of the RF-ready gate, and blank-transfer failure latch the verifier faulted, wipe the code and prevent future issue/verification.
- A failed blank request is not retried by a second display call. The failed transfer remains a failure; it is not treated as evidence that the panel went blank.
- Existing `issue()` and `verify()` paths retain their independent expiry checks. A caller that omits periodic servicing still cannot authenticate an expired code.

Added `lastObservedMicros_` tracking across issue, verify and service calls. This detects a backward monotonic reading relative to the last observation, even if the new reading is still later than the original issue timestamp.

The API is synchronous and expects calls from the future serialized application owner. No timer, FreeRTOS task, mutex, callback or production caller was added. `target/esp32_attendance/main.cpp` and all route/composition paths remain untouched.

## Regression additions

Added nine test definitions and registrations; `test_esp32_ram_setup_code_verifier` now has **25 definitions / 25 registrations**:

1. Inactive service is an idempotent no-op.
2. Before-deadline service retains the active code.
3. Exact-boundary expiry wipes and requests one blank transfer; repeated post-expiry calls do not transfer again.
4. Beyond-boundary expiry also expires without rotating.
5. Clock read failure latches and blanks.
6. Monotonic regression between service calls latches and blanks.
7. RF-gate loss while active latches and blanks.
8. Blank-transfer failure latches, wipes, does not retry the OLED request, and cannot verify the old code.
9. Service after successful code consumption is idempotent.

The tests assert unchanged random-source counters during servicing, code-buffer clearing, post-expiry/fault verification denial and OLED request counts. These are host-fake assertions only; they do not prove physical OLED visibility or panel clearing.

## Exact files and checksums

| Path | Before SHA-256 | After SHA-256 |
|---|---|---|
| `lib/hal_esp32/src/esp32_ram_setup_code_verifier.h` | `0c4f3f174351afc4c5376025d42b2b1d765574b2e657089e8f7ebc6c79d7ec7a` | `cd7b0d617332fd0f569e4b28809ce049f8052d16cf07d8589057e7f2df25e37e` |
| `lib/hal_esp32/src/esp32_ram_setup_code_verifier.cpp` | `c37ed8fd9c9b8fc5e9fd31758eacda4972fe61f4dae39d1f9a7b570bda4209ff` | `6a3842bcc6a621a52295f149b92d8acd4ba48ac7c230a762b76eeb618ee068a8` |
| `test/test_esp32_ram_setup_code_verifier/test_main.cpp` | `7e6a02acee809bc97d566dd2afaed91f8694e6c181fbb36ff9c66b15f3d29893` | `dd9238639268d9ea444088091675add3e3366964cc3a920633904c09c98af2ec` |

Pre-edit copies:
- `comms/staging/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01/before/esp32_ram_setup_code_verifier.h`
- `comms/staging/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01/before/esp32_ram_setup_code_verifier.cpp`
- `comms/staging/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01/before/test_main.cpp`

Unified diff:
- `comms/staging/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01/patch.diff`
- SHA-256: `b9334fb94ed0b1b60a5744f33f727c59bc966faaa75a8bff21603a7c27b3733c`

Diff contains only the three assigned HAL/test paths. All changed files were read back and compared against their pre-edit copies.

## Verification limits and remaining integration

- Source read-back / diff inspection / registration audit: **DONE**.
- Tests, compilers, builds, PlatformIO, flashing and hardware: **NOT RUN / NOT AUTHORIZED**.
- The service is not called by production code. The future serialized application loop must call it regularly; until that owner is implemented and wired, the periodic OLED blanking behavior is not integrated.
- OLED `display()` returns void. A successful method result means a blank-frame transfer was requested, not that physical panel visibility was verified.
- Independent PISHDA review is required before acceptance/integration.
