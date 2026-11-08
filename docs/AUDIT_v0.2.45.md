# Specialized audit — v0.2.45

**Baseline:** clean extraction of `att_v0.2.44_2026-11-08.tar.gz`  
**Scope:** first-provision AP/OLED/config coordinator, AP DHCP assignment observer, updated smoke references, associated tests and documentation.

## 1. Change accounting

A byte comparison against the clean v0.2.44 extraction found:

- 20 pre-existing source/documentation files modified, plus regenerated `MANIFEST.sha256` (21 modified paths total).
- 8 added files, including the release and audit documents.
- 0 deleted files.
- No changes to unrelated AS608 protocol/attendance modules, enrollment FSM, backup/restore logic, or `web/sf2` source.

The 8 additions are:

- `RELEASE_v0.2.45.md`
- `docs/AUDIT_v0.2.45.md`
- `docs/SPECS/ap_credential_setup.md`
- `lib/core/src/ap_credential_setup.h`
- `lib/core/src/ap_credential_setup.cpp`
- `lib/core/src/ap_credential_display.h`
- `lib/core/src/credential_soft_ap.h`
- `test/test_ap_credential_setup/test_main.cpp`

The AP and display ports are split into narrow headers so `Esp32WifiAp` and `Esp32Oled` do not include the full orchestration class just to implement an interface.

## 2. Logic and ordering review

### Positive controls present

- Boot-state gating prevents the coordinator from starting outside `CFG_BOOT_NEEDS_SETUP`.
- SSID/channel validation occurs before credential bootstrap or OLED disclosure.
- The E28 credential is read back and validated before display or AP use.
- OLED transfer request occurs before AP startup.
- AP readiness and a DHCP assignment on the exact AP netif are required before provision.
- The per-request auth object is not stored in the coordinator. A short-lived composite gate forwards the same object to `ConfigBootCoordinator::provision()`.
- The durable AP credential is re-read immediately before provision and written into a private config copy's `wifiPass`. This prevents the previous mismatch hazard where AP and persisted config could use different passwords.
- Sensitive stack buffers are cleared using volatile writes after use.
- A denied second gate check occurs before configuration storage mutation; a regression test covers that fail-closed path.
- Failed post-disclosure cleanup has a retry-only blank-frame state. It cannot start the AP again or write config. A separate post-provision blank retry also never repeats the config write.
- DHCP callback storage is limited to a latched boolean in the AP run's event group; it does not persist client MAC/IP data. The callback filters the event to the exact AP netif.
- The AP callback context is separate from the owner object and is retained if unregister failure prevents safe cleanup, consistent with the earlier lifetime hardening design.

### Remaining design limits / risks

- The authorization object is only as strong as the future caller. There is no HTTP handler or authenticated session to prove that the gate belongs to the current request. The same gate is checked twice and must be side-effect-free/idempotent.
- `IP_EVENT_AP_STAIPASSIGNED` is historical. The bit is not cleared on station disconnect, is not identity and is not authentication. If current association is a product requirement, connect/disconnect tracking is still needed.
- `Adafruit_SSD1306::display()` returns `void`; a true adapter return only means the transfer was requested. The coordinator can proceed without proof of physical visibility or I2C acknowledgement.
- A failed blank-frame request after disclosure enters a retry-only state, but an accepted blank transfer still does not prove the physical panel cleared.
- `start()` accepts caller SSID and a generic channel 1–13 range; regulatory-country validation remains open. E29 remains OPEN.
- E30 reset/rotation/recovery remains OPEN. No overwrite or erase path was added.
- The coordinator and AP adapters are application-task serialized; no concurrent-call protection was added.
- The production boot/HTTP composition, setup-code display/verifier, PIN verifier/KDF, protected routes and fault-state owner remain absent (C6).

## 3. Test inventory and execution evidence

Source inventory was counted directly from `RUN_TEST(` registrations: **520 C++ registrations across 31 suites**.

Fresh local sanitizer-enabled stand-in executions:

| Suite | Tests | Assertions | Failures |
|---|---:|---:|---:|
| `test_ap_credential_setup` | 18 | 145 | 0 |
| `test_esp32_wifi_ap` | 22 | 315 | 0 |
| `test_esp32_oled` | 13 | 67 | 0 |

Compile flags: `-std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g`, temporary Unity-compatible stand-in. This does not substitute for PlatformIO's Unity runner.

SF2 was unchanged and rerun: `npm test` 257/257 passed. All 25 `.mjs` modules passed `node --check`.

User-reported v0.2.44 evidence: 500/500 PlatformIO native cases and successful expanded ESP32 adapter smoke build (Espressif32 8.0.0, Arduino-ESP32/framework libraries 3.3.12, Xtensa 14.2.0+20260121, Adafruit SSD1306 2.5.17/GFX 1.12.6/BusIO 1.17.4; 24,028 bytes RAM and 350,004 bytes flash). These are prior-release results.

**Not run for v0.2.45:** full PlatformIO native suite, expanded ESP32 target build, physical OLED, Wi-Fi AP, DHCP, first-beacon WPA2/CCMP, NVS power-cut, production HTTP/auth or board-level I2C coexistence.

## 4. Release gates

- E28: SIGNED; no policy change.
- E29: OPEN; production SSID source/naming not selected.
- E30: OPEN; reset/rotation/recovery not designed.
- C6: remains blocking until production boot/HTTP composition, P15 setup-code path, PIN verifier/KDF and current-request session enforcement are implemented.
- C33: direct IDF config-before-start remains the intended AP adapter path; first-beacon security still needs packet capture on hardware.
- U100: DHCP assignment is only a historical AP-run lease fact, not current association/authentication.
- U96: display transfer request is not proof of visible OLED pixels.

## 5. Verdict

The increment is appropriately bounded as a reusable first-provision coordinator and adapter composition layer. The new source tests cover the key sequencing, credential consistency, request-gate recheck and cleanup retry invariants. It is **not ready for production boot integration or device deployment**: the target rebuild and PlatformIO run for v0.2.45 are still required, and E29/E30/C6 plus physical hardware checks remain blockers.
