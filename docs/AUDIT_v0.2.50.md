# Specialized audit — v0.2.50

## Scope reviewed

- Native build boundary for shared ESP32 NVS wrapper.
- AS608 adapter composition and inert smoke-target lifetime/side effects.
- Xtensa ELF inspector branch-target handling.
- Verification sequencing for multiple PlatformIO target configurations.
- Decision/handoff preservation and archive hygiene.

## Findings

### 1. Native/ESP32 compile boundary — corrected

The previous source compiled the NVS wrapper implementation in the native environment, where `esp_partition.h` is unavailable. The first targeted run failed before assertions. The source now guards only the ESP-IDF-specific implementation with `ARDUINO_ARCH_ESP32`; the pure `nvs_startup_guard_policy.h` remains platform-neutral. The targeted native policy suite then passed 9/9, and both ESP32 builds compiled the same guarded translation unit with the ESP32 implementation enabled.

### 2. AS608 composition smoke target — compile/link proven, runtime not proven

The smoke target now retains the raw transport, sensor adapter, occupancy reader, enrollment FSM/session, attendance scan client and `As608Runtime`. The function-local static composition is created in `setup()`, after Arduino startup, to avoid cross-translation-unit initialization against global `Serial2`.

Static review of constructors and target setup confirms this target does not call `Serial2.begin()`, issue AS608 requests, start Wi-Fi, request entropy, open application NVS handles, or invoke enrollment/attendance operations. The target is intentionally inert. It is not a firmware image and must not be flashed.

The target's RAM use increased to 63,652 B (19.4%); flash is 926,248 B (70.7%). This is a link-smoke composition footprint, not a production firmware budget.

### 3. NVS guard call routing — linked route proven, policy remains incomplete

Both exact final ELFs passed the revised inspector. The inspector verifies `initArduino()` calls the wrappers, wrapper delegates exist, and three standard NVS erase entry points reach the wrapped erase boundary.

The first inspection falsely failed on the erase wrapper. Direct disassembly showed the delegate call in a separate internal basic block after a conditional branch. Xtensa variable-length instruction decoding meant linear disassembly from the entrypoint was insufficient. The inspector now discovers internal `symbol+offset` branch labels and disassembles those addresses; a dedicated fixture ensures this case remains covered.

**Residual risk:** the wrapper blocks only the exact full-partition request. It permits a sequence of smaller requests whose total could erase the partition; other direct/low-level flash erase APIs may bypass it. Also, a standard NVS erase API can deinitialize handles before the wrapped erase is rejected. C34 must remain blocking.

### 4. Verification workflow — sequencing defect corrected

Both PlatformIO configs share `.pio/build`. Building the probe after the smoke target can replace the earlier target output. The previous workflow built both and only then inspected both ELF paths, so it was not reliable. The new workflow inspects each image immediately after its build. The `links` mode requires a source fingerprint from `targeted` mode to prevent stale evidence after source edits.

The five affected native suites and all 11 inspector fixtures passed. An equivalent targeted workflow completed on the same C++ source state and reported `TARGETED_VERIFICATION=PASS` (90/90 in 1:34.299). A separate attempt to run the new `verify_v0.2.50.sh targeted` wrapper lost its remote process session before its own final summary/marker, so that exact wrapper is not claimed as end-to-end passed. Its shell syntax passed. The underlying two target builds and ELF inspections were run individually and passed. The new `links` orchestration script itself was not rerun because that would repeat both cold ESP32 builds (more than 36 minutes combined on the phone); this procedural limitation is recorded, not presented as an execution result.

### 5. Product decisions and safety boundary — unchanged

- `HANDOFF_FINAL.md` unchanged.
- `docs/DECISIONS.md` unchanged. No SIGNED or DELEGATED item was modified.
- No production startup, recovery workflow, erase firewall, HTTP route, or attendance behavior was introduced.
- No device was flashed and no NVS corruption was induced.

## Verification evidence

- Five native suites: 90/90 passed.
- Inspector fixtures: 11/11 scenarios passed.
- Inert adapter ESP32 build: SUCCESS.
- Diagnostic probe ESP32 build: SUCCESS.
- Smoke final ELF: `NVS_WRAPPER_ROUTING=PASS`.
- Probe final ELF: `NVS_WRAPPER_ROUTING=PASS`.
- Physical device: not tested.
