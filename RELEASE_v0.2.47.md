# v0.2.47 — isolated diagnostic hardware probe

This is a development snapshot, not production firmware. It adds a separate ESP32 hardware-probe target for the shared I2C bus, 128x32 OLED, SoftAP configuration and DHCP event. It also adds a probe-only NVS startup guard in response to C34.

**Critical:** Arduino-ESP32 3.3.12 may erase the entire default NVS partition before `setup()`. Do not flash the probe until its target build succeeds and the final ELF/map proves the Arduino core call sites are redirected through both `__wrap_nvs_flash_init` and `__wrap_esp_partition_erase_range`; symbol-name presence alone is insufficient. On device, require `SAFE_NVS_INIT=PASS`. A failure must abort; never erase NVS to make the probe pass.

The probe generates a temporary WPA2 passphrase, prints it over USB serial and shows it on the OLED. Do not reuse it. The serial log contains this credential: keep the log private, redact the `PROBE_PASSWORD` line before sharing diagnostics, and delete the local log after the probe. The probe does not run attendance scans, start HTTP, mount attendance storage, or deliberately access application config/attendance namespaces. The Wi-Fi PHY may use NVS namespace `phy`.

The added `tools/inspect_probe_wrappers.sh` checks the final ELF's `initArduino()` call targets and verifies that each wrapper delegates to its real IDF function. It has only passed shell syntax validation here; the target ELF is not available. No v0.2.47 PlatformIO test or target build has been run by the assistant. Static inventory is 528 C++ test registrations across 32 suites; eight new probe-helper registrations are pending. No probe flashing is authorized until the user's build and final ELF inspection have both passed and the result has been reviewed. The user-provided v0.2.46 result remains 520/520 native cases and successful inert adapter smoke compile/link only.

Install only by replacing the complete project tree from this archive. Never merge selected files into an older project.
