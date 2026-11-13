# Specialized audit — v0.2.47

Date: 2026-11-08
Base: complete `att_v0.2.46_2026-11-08.tar.gz` uploaded by the user.

## Scope

New files: `target/esp32_hardware_probe/main.cpp`, `target/esp32_hardware_probe/probe_credential.h`, `target/esp32_hardware_probe/probe_nvs_guard.h`, `platformio.esp32-probe.ini`, `test/test_hardware_probe/test_main.cpp`, and `tools/inspect_probe_wrappers.sh`. The eight tests were authored before the two pure helper implementations. Updated release/context/critical/unverified/test/changelog documentation and the README. No existing core module, sensor driver, SF2 source, signed handoff rule or existing native test suite was changed.

## Main safety finding (C34)

Pinned Arduino-ESP32 3.3.12 `app_main()` calls `initArduino()` before creating the loop task and before the sketch's `setup()`. `initArduino()` calls `nvs_flash_init()` and attempts a full default DATA_NVS erase on `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`. Therefore, testing NVS only from `setup()` cannot prevent the destructive fallback.

The probe now wraps `nvs_flash_init()` to capture the first result and `esp_partition_erase_range()` to deny only `(DATA_NVS, offset=0, size=partition->size)`. Normal smaller NVS sector erases are allowed. This preserves the rest of the original Arduino `initArduino()` startup logic, unlike the first draft that replaced `initArduino()` wholesale. If NVS initialization fails or the full-partition erase is attempted, `setup()` aborts before I2C, OLED or AP. The link map and final ELF call references must verify actual wrapper redirection before flashing; merely finding wrapper symbol strings is not enough. The pinned ESP-IDF v5.5.5 header declares `esp_partition_erase_range(const esp_partition_t*, size_t, size_t)`, and its `esp_err.h` defines `ESP_ERR_NOT_ALLOWED`. This is a probe-only guard, not the production startup fix.

## Probe behavior

- `Wire.begin(21,22,100000)` initializes the shared I2C bus once. The probe scans 0x08..0x77 and reports ACKs for expected OLED 0x3C, EEPROM 0x57 and RTC 0x68. It does not invoke RTC/EEPROM protocol operations.
- Existing `Esp32Oled` is used to request the setup screen and later blank frame. Its `display()` call is void; the serial status is not proof of visible pixels. The user must inspect the screen.
- A temporary 128-bit random passphrase is generated before Wi-Fi starts. The password is output to USB serial as a deliberate fallback and displayed on the OLED. It is never written by the probe to NVS/Config. It must not be reused. Serial captures are secret-bearing: redact `PROBE_PASSWORD` before sharing logs and delete local captures after the test.
- Existing `Esp32WifiAp` is used with `att`, channel 1 (E31 delegated), WPA2-PSK and CCMP. After AP start, `esp_wifi_get_config()` is compared against the actual expected SSID, ephemeral password, channel, auth, cipher, max-client limit and visible-SSID setting. A mismatch makes the probe stop the AP and exit. This does not inspect the first over-the-air beacon; U93 remains open.
- DHCP output means this AP netif saw `IP_EVENT_AP_STAIPASSIGNED`; it is a historical lease event, not user identity or current connection proof.
- No HTTP server, AS608 command, attendance log, config backend, restore path or PIN coordinator is started. Wi-Fi PHY may read/write NVS namespace `phy`; claiming that the probe makes no NVS writes would be false.

## Critical review / residual risk

1. **Linker wrapper routing is unverified.** A successful link or wrapper symbol listing is insufficient. The new `tools/inspect_probe_wrappers.sh` checks that `initArduino()` calls both wrappers and each wrapper delegates to its real IDF function. It passed `bash -n` source syntax validation here, but it cannot be run against the target ELF until the user builds the probe.
2. **No failure-injection on the user's actual NVS.** Do not corrupt or downgrade NVS just to exercise the failure branch. The source guard is tested as a pure predicate by host tests, but its actual link routing is pending.
3. **No compile or hardware execution here.** The assistant cannot compile/run this project or inspect a real board. The eight new native registrations and the dedicated target remain unrun.
4. **Temporary credential exposure.** The password appears in serial logs and on the OLED. This is acceptable only for a supervised diagnostic probe and must never be reused or treated as a production credential.
5. **NVS scope is deliberately limited.** The probe does not access application config/attendance namespaces. NVS initialization still reads the partition; the Wi-Fi PHY can access `phy`; normal sector erases remain permitted.
6. **AP channel 1 remains delegated, not reviewed.** Philippines regulatory/country state is still U93.
7. **OLED transfer remains request-only.** No ACK/pixel feedback is available from the current Adafruit adapter.
8. **DHCP is not authentication.** The existing latched event is historical only.

## Test inventory and evidence

- Static inventory: **528 C++ `RUN_TEST` registrations across 32 suites** (520 across 31 before this increment).
- New suite adds eight registrations. They have not been run.
- User reports v0.2.46: 520/520 native cases in 00:08:06.877 and `esp32-adapter-link` build success in 548.96 sec. Target memory reported: RAM 47,012 B (14.3%); flash 912,216 B (69.6%). This does not cover v0.2.47.
- No v0.2.47 compile, PlatformIO test, link-map inspection, flash or device behavior is claimed.

## User verification batch

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native 2>&1 | tee native-test-v0247.log
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link 2>&1 | tee esp32-smoke-v0247.log
pio run -c platformio.esp32-probe.ini -e esp32-hardware-probe 2>&1 | tee esp32-probe-build-v0247.log
./tools/inspect_probe_wrappers.sh
```

Run the inspection only after the probe target build succeeds. It must print `PROBE_WRAPPER_ROUTING=PASS`; review its output before considering a flash. Even that verifies only linked call routing, not the runtime safety behavior. The probe is diagnostic-only; do not use its image as attendance firmware.


## Pinned API references

- Arduino-ESP32 3.3.12 startup order and NVS fallback: `cores/esp32/main.cpp` and `cores/esp32/esp32-hal-misc.c` at https://github.com/espressif/arduino-esp32/tree/3.3.12/cores/esp32 (the `app_main()` → `initArduino()` path and pre-`setup()` whole-partition erase branch).
- ESP-IDF 5.5.5 `esp_partition_erase_range()` declaration: https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_partition/include/esp_partition.h.
- ESP-IDF 5.5.5 `ESP_ERR_NOT_ALLOWED` definition: https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_common/include/esp_err.h.
