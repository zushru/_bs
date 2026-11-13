# Release v0.2.45 — first-provision AP/OLED/config orchestration

**Date:** 2026-11-08  
**Status:** Development increment; not production firmware; target rebuild and all device behavior remain unverified.

## Scope

This increment adds a reusable, hardware-independent first-provision coordinator that connects the previously separate E28 AP credential bootstrap, OLED disclosure adapter, WPA2 SoftAP adapter and initial config provisioner. It also adds DHCP-assignment observation to the AP adapter and keeps the smoke target's references to the complete new call chain.

### Ordered first-provision flow

1. Enter only when the boot coordinator reports `CFG_BOOT_NEEDS_SETUP`.
2. Validate an explicit caller-provided SSID (1–32 non-NUL bytes) and channel (1–13) before reading or disclosing the stored secret. No SSID default is invented; E29 remains OPEN.
3. Ask `ApCredentialBootstrap` for the durable E28 passphrase and validate the expected 32-character lowercase-hex representation.
4. Request the OLED disclosure frame before starting the AP.
5. Start the WPA2-PSK/CCMP AP and wait for its AP-start-ready state.
6. Wait for a latched `IP_EVENT_AP_STAIPASSIGNED` event that identifies this AP netif.
7. Require an authorization-gate object supplied to the current synchronous `provision()` call.
8. Re-read the durable AP credential and overwrite `wifiPass` in a private copy of the initial `Config`. The coordinator will not commit a caller-supplied passphrase that differs from the actual AP credential.
9. Pass the config through `ConfigBootCoordinator::provision()`, then request an OLED blank frame. The AP remains running under the signed always-on policy.

### Failure handling and security constraints

- If a credential may have been disclosed and startup/read-back/provisioning fails, the coordinator requests a blank frame.
- If a fault-path blank request is rejected, state becomes `ACSS_STARTUP_CLEAR_PENDING`. Retrying this state can only request a blank frame; it cannot restart the AP or provision config.
- If config provision succeeded but the blank request is rejected, `ACSS_CLEAR_PENDING` retries only the display blank request and never repeats the config write.
- Authorization is supplied per call and is not retained. The coordinator preflights the gate and `ConfigProvisioner` checks it again before storage mutation. The gate must therefore be side-effect-free/idempotent and constructed by the future HTTP handler from the current authenticated request/session. The implementation cannot prove caller provenance.
- The DHCP assignment flag is historical for the AP run. It does not prove that the station remains connected, identify the client or authenticate it. No MAC/IP address is retained.
- OLED `display()` returns `void`; a true adapter result means a transfer was requested, not that I2C was acknowledged or pixels were visible.
- Narrow interface headers (`ap_credential_display.h` and `credential_soft_ap.h`) keep the HAL drivers from including the full setup coordinator header.

## Test evidence

### Local supplemental runs in this increment

All three C++ suites were freshly compiled with `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g` and run using a temporary Unity-compatible stand-in. They are not PlatformIO/Unity results.

| Suite | Result | Assertions |
|---|---:|---:|
| `test_ap_credential_setup` | 18/18 | 145 |
| `test_esp32_wifi_ap` | 22/22 | 315 |
| `test_esp32_oled` | 13/13 | 67 |

The setup suite includes a regression where a gate allows the coordinator preflight but denies the core's immediate pre-write check; configuration remains unmodified. It also covers retry-only cleanup after a failed blank-frame request.

The unchanged SF2 Node suite was rerun with `npm test`: **257/257 passed**. `node --check` passed for all **25** `.mjs` files.

Current C++ source inventory: **520 `RUN_TEST` registrations across 31 suites**. The user reports the preceding v0.2.44 snapshot passed **500/500** PlatformIO native cases. The 20 new v0.2.45 registrations (18 coordinator and 2 AP DHCP-event cases) have only local stand-in execution so far. A registration count is not a pass count.

### Pending project-environment verification

The v0.2.45 full PlatformIO native suite and expanded ESP32 smoke build have **not** been run in this packaging environment. The user reports that v0.2.44 passed 500/500 native cases and its target smoke build linked with PlatformIO Espressif32 8.0.0, Arduino-ESP32/framework libraries 3.3.12, esptool 5.4.0, Xtensa toolchain 14.2.0+20260121, Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4. Those results do not cover the v0.2.45 coordinator or DHCP callback.

No physical OLED, I2C, SoftAP, DHCP, first-beacon WPA2, NVS brownout, setup-auth, or production boot/HTTP test was performed.

## Files changed from v0.2.44

Compared with a clean extraction of v0.2.44:

- **20 pre-existing source/documentation files modified**, plus regenerated `MANIFEST.sha256` (**21 modified paths total**): README/release and project status records; OLED and Wi-Fi AP adapter interfaces/implementations; smoke application; AP suite and its ESP-IDF/FreeRTOS stubs.
- **8 files added**: `RELEASE_v0.2.45.md`, `docs/AUDIT_v0.2.45.md`, `docs/SPECS/ap_credential_setup.md`; `lib/core/src/ap_credential_setup.{h,cpp}`; narrow ports `lib/core/src/ap_credential_display.h` and `credential_soft_ap.h`; and `test/test_ap_credential_setup/test_main.cpp`.
- **0 files deleted.**
- No unrelated attendance core, AS608 sensor protocol, enrollment FSM, backup/restore, or SF2 implementation was changed.

## Decisions and blockers

- E28 remains SIGNED and unchanged.
- E29 (production SSID source/naming) remains OPEN.
- E30 (credential reset, rotation and forgotten-password recovery) remains OPEN.
- C6 remains blocking: no production boot caller, setup-code/PIN verifier, KDF policy, protected HTTP route or authenticated session integration exists.

## Required verification commands

Run after replacing the whole project tree from this archive:

```bash
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_ap_credential_setup -vvv 2>&1 | tee ap-credential-setup.log
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap.log
pio test -e native 2>&1 | tee native-test.log
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link 2>&1 | tee esp32-target-smoke.log
(cd web/sf2 && npm test)
```

The smoke image is compile/link-only. **Do not flash it as production attendance firmware.**
