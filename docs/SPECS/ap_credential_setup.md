# AP first-provision setup coordinator

Status: **v0.2.45 core/adapter composition only; not a production boot flow**.

## Purpose

`ApCredentialSetupCoordinator` owns the ordering between the signed first-boot AP
credential bootstrap, OLED transfer request, secured SoftAP start and first config
provision. It is hardware-independent and receives narrow display/AP interfaces.

It does not select an SSID, implement the setup-code/PIN authentication mechanism,
create HTTP routes, or offer passphrase rotation/reset/recovery.

## Startup sequence

`start(ssid, ssidLength, channel)` is accepted only while the coordinator is COLD
and `ConfigBootCoordinator` reports `CFG_BOOT_NEEDS_SETUP`.

Before reading or generating a secret, it validates that the caller supplied a
non-null, non-empty SSID of at most 32 bytes, with no embedded NUL, and channel
1–13. This prevents disclosing a password for an AP configuration that the ESP32
adapter would reject. It does not establish that the channel is legal in the
selected regulatory domain; U93 remains open.

The successful sequence is:

1. `ApCredentialBootstrap::prepare(CFG_BOOT_NEEDS_SETUP, nullptr, ...)` creates
   or reloads the durable 128-bit / 32-character lowercase-hex secret and performs
   its NVS read-back check.
2. The coordinator asks `IApCredentialDisplay` to transfer the disclosure frame.
3. Only after that request returns true does it start `ICredentialSoftAp` with the
   same explicit SSID, credential and channel.
4. The AP adapter reports ready only after its AP_START event and exposes a
   non-PII fact that the AP's own DHCP server assigned a client address.
5. Provisioning remains blocked until AP readiness, that DHCP-assignment fact,
   and an `IConfigProvisionGate` supplied by the caller for that specific provision request all pass.

Any bootstrap failure before disclosure latches FAULT. After a credential frame
may have been sent, display/AP-start, bootstrap read-back or ambiguous storage
failure requests a blank frame before latching FAULT. If the blank-frame request
fails, state becomes `ACSS_STARTUP_CLEAR_PENDING`; `retryBlankFrame()` can retry
only the blank frame and then transitions to FAULT. It cannot restart the AP or
provision config. The Adafruit API does not confirm physical panel clearing.

## Provisioning and credential consistency

`provision(initial, requestAuthGate, bootResult)` accepts an authorization gate for the current synchronous request; it never retains or caches that gate. A short-lived composite gate passes the same request authorization and AP/DHCP conditions into `ConfigBootCoordinator::provision()`. The future HTTP handler must construct the object from the authenticated current request/session; the coordinator cannot verify the gate's provenance.

`provision()` does not use the caller's `initial.wifiPass`.
After checking the AP, DHCP and setup-auth gates, it re-reads the durable bootstrap
credential, validates the exact 32-character lowercase-hex format, copies the
other initial configuration fields, and replaces only `wifiPass` in a private
configuration copy. It then calls `ConfigBootCoordinator::provision()` with a
short-lived composite gate that forwards the same request authorization and
AP/DHCP conditions. The local plaintext copies are explicitly cleared after use.

This is essential: if the AP used the generated credential but first provision
persisted a different caller-supplied `Config::wifiPass`, the next `CFG_BOOT_READY`
boot would reject the mismatch against `attwifi/ap_pass_v1` and strand the device.
The regression test `test_provision_persists_the_exact_ap_secret_not_the_callers_value`
asserts that the config, AP and NVS store all use the same value.

After durable provisioning succeeds, the OLED is asked to display a blank frame.
If that request fails, state becomes `ACSS_CLEAR_PENDING`; `retryBlankFrame()`
retries only that display request and never repeats configuration storage. The AP
remains running, consistent with the signed always-on policy. A distinct
`ACSS_STARTUP_CLEAR_PENDING` state covers failed setup/provision cleanup; after its
blank request succeeds, the coordinator remains FAULTed and cannot continue setup.

## AP DHCP event handling

`Esp32WifiAp` registers the pinned ESP-IDF `IP_EVENT_AP_STAIPASSIGNED` handler
before starting Wi-Fi. The event's `esp_netif` pointer must match this AP's netif.
The callback records only a latched assignment bit in the current AP run; it does
not copy the station MAC or assigned IP into application state. A new AP start
gets a fresh event group, and `hasAssignedClientIp()` is false after stop/fault.

The assignment is historical for the current AP run; it is not proof that the
station is still connected, not an authenticated identity, and not an optical
OLED acknowledgement. The per-call setup-auth gate is still mandatory. A future HTTP composition must
construct that gate from the authenticated setup action for the current
provisioning request; a global stale boolean is not sufficient. Its
`mayProvision()` must be side-effect-free/idempotent because the coordinator
checks it before credential read-back and the core provisioner checks the same
request gate again immediately before any storage mutation.

## State machine

- `ACSS_COLD`: no start attempted; invalid caller SSID/channel can be corrected.
- `ACSS_WAITING_FOR_CLIENT`: OLED transfer requested and secured AP ready; waits
  for AP DHCP assignment and successful setup authorization.
- `ACSS_CLEAR_PENDING`: config is READY; only the post-provision OLED blank-frame retry is allowed.
- `ACSS_STARTUP_CLEAR_PENDING`: startup/provision fault occurred after the secret may have been disclosed; only a blank-frame retry is allowed, then state becomes FAULT.
- `ACSS_PROVISIONED`: config is READY and blank-frame transfer was requested.
- `ACSS_FAULT`: setup cannot continue in this coordinator instance. Either no secret was disclosed, or a blank-frame request was accepted after a fault; acceptance does not prove visible pixels were cleared.

Calls are serialized on one application task. The coordinator must outlive its referenced boot, bootstrap, display and AP
dependencies. The request authorization object must remain alive only through
the synchronous `provision()` call; it is not retained after return.

## Tests and limits

`test_ap_credential_setup` now has 18 registrations and covers boot-state and invalid-argument gating,
display-before-AP order, display/AP failures, retry after a failed blank-frame request,
DHCP/AP/auth gating, request-scoped authorization, credential consistency,
correctable invalid config, ambiguous storage failure and post-provision blank-frame
retry. `test_esp32_wifi_ap` has 22 registrations and covers AP-netif-only DHCP
assignment, unrelated-netif rejection, event cleanup, registration rollback and
callback-context lifetime.

The v0.2.45 focused core suite passed with a temporary Unity-compatible stand-in
under ASan/UBSan (18 tests, 145 assertions), not PlatformIO Unity. The updated AP
suite passed the same way (22 tests, 315 assertions); the unchanged OLED suite
passed 13/13 (67 assertions). The user still needs to run the full PlatformIO
native suite and target smoke build on the project environment.
No physical OLED, DHCP, SoftAP, NVS power-cut or HTTP setup-auth test has been run.

## Explicitly out of scope

- Production `setup()` / boot entry point and complete dependency composition.
- E29 fixed SSID is signed; E31 channel 1 is delegated. Regulatory-country configuration remains unverified (U93).
- E30 credential rotation, reset and forgotten-password recovery.
- Six-digit setup-code generation/display and production setup-auth verifier.
- HTTP server, request-scoped authenticated session enforcement and protected routes.
- Proof of actual OLED pixel visibility or I2C acknowledgement.
- First-beacon WPA2/CCMP capture and live AP/DHCP verification.
