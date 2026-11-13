# AP credential bootstrap contract

Status: v0.2.41 core and NVS adapter modules implemented; no production boot caller or OLED driver exists. E28 (credential source and disclosure contract) is SIGNED. E29 is SIGNED (`att`); E31 is DELEGATED (channel 1). E30 is DELEGATED to no remote reset/rotation; USB service recovery is not implemented. Host tests use a Unity-compatible stand-in and are not PlatformIO or ESP32 verification.

## Security and product requirement

The SoftAP must never start open and must never use the six-digit setup code, a diagnostic default, or a hard-coded shared password as its WPA2 passphrase. The initial passphrase is device-specific and generated from 128 random bits. It is encoded as 32 lower-case hexadecimal characters (32 ASCII bytes), valid for the WPA2-PSK passphrase policy already enforced by `Esp32WifiAp` and `Config::wifiPass`.

The secret is persisted before AP startup. The production setup caller must display it on the OLED before moving configuration from `CFG_BOOT_NEEDS_SETUP` to `CFG_BOOT_READY`. The `disclose` result is a request, not proof that the screen was rendered. Do not expose the passphrase through public `GET /status`, HTTP responses, serial logs or diagnostic telemetry. Clear temporary copies after AP configuration and after display.

## Boot behavior

### `CFG_BOOT_NEEDS_SETUP`

1. Read NVS namespace `attwifi`, key `ap_pass_v1`.
2. If a valid WPA2 passphrase exists, reuse it. Do not generate a new one. Return `APCB_OK` with `disclose=true`.
3. If and only if the key/namespace is confirmed missing, obtain 16 bytes from the early-boot entropy adapter, encode to 32 lower-case hex characters, and call the store's create-if-absent method.
4. Read the committed value back and validate it before returning it. If another writer won a create race, use the value actually stored, not the discarded candidate.
5. Return `disclose=true` on every boot that remains NEEDS_SETUP. This lets the OLED setup screen repeat the same stored value after a reset before provisioning is complete.
6. Any NVS read fault, malformed value, random failure, write failure or read-back failure blocks AP startup. Never auto-erase or overwrite a bad record.

### `CFG_BOOT_READY`

1. Require the validated `Config::wifiPass`; it is the authoritative active credential.
2. If `attwifi/ap_pass_v1` exists and matches exactly, use it and return `disclose=false`.
3. If the separate NVS key is confirmed missing, create it from `Config::wifiPass`, commit, read back and compare. This supports an upgrade of an existing installation and restoring a config backup to a device whose `attwifi` key is absent.
4. If an existing value differs, is malformed or cannot be read, fail closed. Do not silently rotate either the config credential or the NVS record.
5. Do not disclose the passphrase automatically in READY mode.

## Entropy source

`Esp32BootstrapRandom` calls `bootloader_random_enable()`, fills the requested output using `esp_fill_random()`, then calls `bootloader_random_disable()`. ESP-IDF 5.5.5 documents this as enabling the SAR-ADC entropy source for true random output when RF is not active. The call must happen before Wi-Fi/Bluetooth/ADC/I2S activity and must be serialized with those subsystem initializations. The adapter is synchronous; its IDF functions return `void`, so it cannot report an internal hardware entropy fault.

This adapter is not the same as `Esp32SecureRandom`: that adapter is deliberately gated until AP RF is active and remains the token-session RNG. `Esp32BootstrapRandom` is only for the initial pre-radio credential. Exact Arduino-ESP32 component linkage and target behavior remain [UNVERIFIED: a target build against the installed package and a first-boot device test].

## NVS layout and backup interaction

- Namespace: `attwifi` (separate from config namespace `attcfg` and the still-DELEGATED PIN counter namespace).
- Key: `ap_pass_v1`, NVS string containing a NUL-terminated 8–63 printable-ASCII passphrase.
- Store API: `load()` distinguishes confirmed not-found from all other errors; `createIfAbsent()` refuses to replace a valid or malformed existing key; successful writes call `nvs_commit()`. The core then performs a fresh read-back.
- NVS is initialized by the existing config backend before this adapter is called. No automatic partition erase/reformat is allowed.
- The config model continues to contain `Config::wifiPass`; the config/backup payload therefore remains the portable recovery source. The standalone `attwifi` key is not part of the backup. A blank target can migrate from the restored config. A target with a different existing `attwifi` value fails closed; the future restore coordinator must define a safe reconciliation path.
- No NVS encryption or physical-flash confidentiality guarantee is claimed; the signed R11 threat model excludes physical flash access.

## Required production ordering

1. Initialize NVS via the existing config backend.
2. Run `ConfigBootCoordinator::start()` and retain its exact state.
3. Construct one `Esp32NvsApCredentialStore`, one `Esp32BootstrapRandom` and one `ApCredentialBootstrap`.
4. Call `prepare(state, state == CFG_BOOT_READY ? config.wifiPass : nullptr, passphraseBuffer)`. Stop on every non-`APCB_OK` result.
5. If `disclose` is true, render the same passphrase on the OLED and keep setup incomplete until the setup flow has presented it. The code currently does not have a durable OLED acknowledgment flag; this sequencing belongs to the production caller.
6. Use the fixed E29 SSID `att` and delegated E31 channel 1, then pass them and the passphrase to the sole `Esp32WifiAp` owner. WPA2-PSK/CCMP must be installed before `esp_wifi_start()`.
7. Clear the temporary passphrase buffer after the AP adapter has copied/configured it. Keep other secret-bearing buffers out of logs and HTTP responses.
8. During first setup, persist that same passphrase in `Config::wifiPass` before changing the boot state to READY.

No firmware boot composition currently implements these steps. Do not claim the AP is ready for first provision merely because the standalone modules exist.

## Explicit non-goals / OPEN decisions

- Production first-provision SSID is fixed to `att` (E29); channel 1 is delegated under E31.
- No AP passphrase rotation/reset endpoint or automatic erase is allowed (E30). A physically supervised USB service recovery procedure is not implemented.
- No password disclosure over HTTP or serial is implemented.
- No production OLED UI or display acknowledgment mechanism is implemented.
- No guarantee of NVS power-loss behavior is made without target tests.
