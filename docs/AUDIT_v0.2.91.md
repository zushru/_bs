# Critical audit — v0.2.91 C1/C34 NVS access order

## Verified change

The AP credential and PIN-attempt stores can no longer be constructed without a reference to a `ConfigBootCoordinator`. Each operation checks the safe pre-setup Arduino NVS snapshot, then checks the currently referenced coordinator is `READY` or `NEEDS_SETUP` before opening NVS. The PIN runtime stores the same coordinator reference used for its internal attempt store. The two focused NVS suites pass 65/65, including explicit `COLD` and `FAULT` no-open cases.

## Adversarial review

1. **This is not a security boundary against arbitrary code.** It prevents accidental use through these adapter APIs before config boot. It does not prevent a future component from calling `nvs_open()` directly or constructing a separate coordinator. The production root must expose only the authoritative adapters/coordinator and keep raw NVS calls private.
2. **The reference has a lifetime contract.** `ConfigBootCoordinator` must outlive `Esp32NvsApCredentialStore`, `Esp32NvsPinAttemptStore`, and `Esp32PinAuthRuntime`. The smoke global order was updated accordingly. There is no ownership transfer or runtime lifetime check.
3. **State synchronization remains single-task only.** The coordinator state is read on every public adapter operation. This assumes all calls are serialized on the documented boot/application task; concurrent provisioning and NVS use is not made safe by this patch.
4. **READY/NEEDS_SETUP permission is intentional.** First-time setup must access the AP credential and PIN-attempt stores while config is `NEEDS_SETUP`. READY permits normal credential/counter operations. COLD/FAULT denies all these adapter calls.
5. **Startup erase guard remains narrow.** The exact wrapped whole DATA_NVS erase check does not become a complete flash-erase firewall. Smaller/chunked ranges, DATA_NVS_KEYS, or direct lower-level flash erase routes remain outside the exact guard.
6. **No production adoption.** There is still no real main/scheduler that constructs and starts the one authoritative `ConfigBootCoordinator`, then passes it into AP/PIN adapters before services are exposed. The changed smoke `main.cpp` object compiled, but the target build stopped before the changed HAL TUs/final link. No final ELF, production firmware, or runtime check exists.
7. **NVS durability is unchanged and unproven.** No NVS key/format changed; no corruption, migration, erase, power-cut or brownout experiment was run. Existing host fake-driver commit tests do not prove target persistence.

## Evidence

- Test-first red: constructor static assertions failed as expected in `test_nvs_ap_credential` and `test_nvs_config`; `/tmp/att_c1_nvs_gate_red.log`.
- Green: `pio test -e native -f 'test_nvs_ap_credential' -f 'test_nvs_config'` — 65/65, exit 0; `/tmp/att_c1_nvs_gate_green3.log`.
- Target attempt: `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link`; target `main.cpp.o`, bootloader and partition artifacts completed, but build stopped in Adafruit GFX before changed HAL TUs/final link; `/tmp/att_c1_nvs_gate_esp32.log`.

**Disposition:** the direct AP/PIN adapter early-call bypass is closed under its documented single-task/reference-lifetime contract. C1/C34 remain blocking pending production-root adoption, broader erase-path coverage, exact target link/ELF review, and physical runtime/power-fault evidence.
