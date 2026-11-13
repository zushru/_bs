# Specialized audit — v0.2.46

**Baseline:** complete v0.2.45 archive extraction.
**Scope:** E29 production first-provision SSID/channel, OLED disclosure label, E30 delegated policy, coordinator API, target smoke signature, tests and current documentation.

## 1. Change summary

Static comparison against the v0.2.45 extraction: **17 modified files, 2 added files, 0 deleted files** (19 changed paths before manifest regeneration). No unrelated AS608 protocol, attendance core, enrollment FSM, backup/restore, or `web/sf2` source was changed.

- `ApCredentialSetupCoordinator::start()` no longer accepts caller-supplied SSID or channel. It uses the fixed product constants `att` and channel 1.
- The OLED now displays `Wi-Fi: att` above the 32-character random WPA2 passphrase. The screen still cannot prove physical visibility because the Adafruit `display()` method returns `void`.
- The target smoke project retains the updated no-argument `start()` member-function pointer.
- E29 is SIGNED (`att`). E31 is DELEGATED to fixed channel 1; target regulatory acceptance remains unverified. E30 is DELEGATED to no remote reset/rotation, no reset gesture, and no automatic erase. USB service recovery is explicitly not implemented.
- The core source registration count remains 520 across 31 suites; one existing coordinator test was replaced/updated and one existing OLED assertion was updated. No new test registrations were added.

## 2. Critical review

### Positive

- The production coordinator no longer accepts a caller-controlled SSID, so its first-provision path cannot accidentally advertise a school/MAC/owner identifier.
- The fixed network label is shown on the same OLED disclosure screen as the password, helping identify the intended device when multiple devices named `att` are nearby. The random per-device WPA2 passphrase remains the actual discriminator.
- Channel 1 is selected as a delegated fixed default rather than allowing arbitrary channels 1–13 through the first-provision coordinator.
- No unsafe reset path was added. In particular, the code does not erase the `attwifi` NVS namespace or mutate `Config::wifiPass` independently.
- The adapter-level `Esp32WifiAp` remains generic; only the production setup coordinator fixes the product policy.

### Remaining risks / limitations

- All nearby devices advertise the same SSID `att`. This is intentional per user instruction, but the admin must select the AP using the passphrase displayed on the intended device.
- Channel 1's acceptance under the actual ESP-IDF regulatory-country state is not verified. U93 remains open; the production Wi-Fi composition must set/confirm a suitable regulatory configuration.
- E30's conservative policy avoids unsafe reset/rotation, but it leaves no implemented recovery path for a forgotten, corrupt, or mismatched credential. USB service recovery is a requirement, not an existing feature.
- The OLED change is only a transfer request. No physical panel acknowledgement exists.
- `IP_EVENT_AP_STAIPASSIGNED` remains historical evidence only; it does not prove a currently connected or authenticated station.
- C6 remains blocking: there is no production boot owner, six-digit setup-code verification, request-bound gate construction, PIN verifier/KDF, protected HTTP route layer, or `src/main.cpp`.

## 3. Verification accounting

- Source inventory: 520 C++ `RUN_TEST` registrations across 31 suites (unchanged).
- No C++ compiler, PlatformIO native run, ESP32 target build, Node test, or hardware test was executed for v0.2.46 in this environment.
- v0.2.45's local ASan/UBSan stand-in results (setup 18/18, AP 22/22, OLED 13/13) are historical and do not verify the modified v0.2.46 source/tests.
- The user-reported v0.2.44 500/500 PlatformIO result and v0.2.44 target smoke link are prior-release evidence only.

Required user verification: full `pio test -e native`, then `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link` from this complete archive. Do not flash the smoke image as production firmware.

## 4. Release gates

- E28: SIGNED.
- E29: SIGNED (`att`). E31: DELEGATED (channel 1).
- E30: DELEGATED to no remote reset/rotation or automatic erase; USB service recovery not implemented.
- C6: blocking until production boot/setup-code/PIN/HTTP composition and request-bound authorization exist.
- U96/U100/U101: physical display, current-client identity and request-gate provenance remain unverified.
- U93: channel/regulatory-country configuration remains unverified.

## 5. Verdict

The increment is deliberately narrow and reduces caller configuration risk. It does not advance the device to production firmware. **Do not deploy or flash this as production firmware. Run the full native suite and target smoke build before accepting v0.2.46.**
