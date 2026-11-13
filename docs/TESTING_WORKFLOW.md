> Current execution policy (batching, blocker classification, deadline passes and report format): see the root [`DEVELOPMENT_WORKFLOW.md`](../DEVELOPMENT_WORKFLOW.md). This file is the detailed test/build command reference.

# Incremental testing workflow

The development environment is an Android phone under Termux/PRoot. Keep repeated work proportional to the change; do not run all 621 native registrations after every increment.

## Per increment

1. Identify the directly affected native suites before editing.
2. Add or adjust the regression test before changing behavior.
3. Run only affected suites with PlatformIO -f filters. Filters select suite paths, not individual Unity functions.
4. For ESP32-specific changes, compile the smallest affected target. Do not flash the inert adapter smoke.
5. Record exactly which suites and targets ran and their actual results. Registration inventory is not test evidence.

## v0.2.51 commands

Focused tests, shell checks and inspector fixtures:

```sh
./tools/verify_v0.2.51.sh targeted
```

Target builds and final-ELF route checks:

```sh
./tools/verify_v0.2.51.sh links
```

Full native regression, SF2 tests/syntax and both target builds are reserved for integration/release checkpoints:

```sh
./tools/verify_v0.2.51.sh full
```

The v0.2.51 targeted mode runs seven suites: NVS startup policy, probe credential helpers, AS608 adapter, AS608 ownership gate, attendance pipeline, log-backed attendance admission and enrollment. It also runs 11 branch-aware inspector fixtures and shell syntax checks. A source-tree fingerprint is written only after all checks pass.

The links mode checks that the source tree has not changed since the successful targeted run. It then builds and inspects each target ELF immediately before building the next target. This ordering is mandatory because both PlatformIO project configurations reuse .pio/build and a later config can replace the earlier ELF. Native tests are not repeated in links mode.

Target builds use two jobs to limit phone memory pressure. Use PIO_JOBS=1 if needed. No verifier mode uploads or flashes.

## v0.2.57 LittleFS adapter subset

```sh
./tools/verify_v0.2.57.sh targeted
```

This runs `test_littlefs_fileset` and the dependent `test_monthset` suite. The adapter suite uses a host POSIX temporary directory; its `fsync()` and read-back assertions are not ESP32 LittleFS or power-loss evidence. The destructive methods remain disabled. An ESP32 target build is still required before integrating the adapter into firmware.

## v0.2.58 append-failure contract

Run tools/verify_v0.2.58.sh targeted.

This runs test_monthset only. Its fake backend simulates partial writes and a complete write followed by a reported failure. It verifies LogStore latching and replay behavior; it does not inject failures into actual LittleFS VFS calls or prove power-cut durability.

## v0.2.70 smoke-link and ELF verification

The inert `esp32-adapter-link` target now retains `RosterTransitionAudit` and compiles the current read-only AS608 runtime API. The successful v0.2.70 command was:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
```

Inspect the exact resulting ELF before another PlatformIO project configuration replaces the shared `.pio/build` tree:

```sh
tools/inspect_probe_wrappers.sh "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
```

`tools/inspect_probe_wrappers.sh` is executable in this release. The `rosterAuditSmoke` symbol measured 3,628 bytes in Xtensa BSS; the complete smoke image reports 67,308 bytes RAM and 927,120 bytes application flash. These are target compile/link measurements only. Do not flash this image.

## v0.2.72 host append fault-injection coverage

The v0.2.72 increment adds a per-instance native-only syscall seam used only by test_littlefs_fileset. It redirects appendFile write, fsync, verification pread and close calls for deterministic POSIX fault injection. The ESP_PLATFORM branch retains direct calls and the host test header deliberately fails compilation if included under ESP_PLATFORM.

Exact owning command:

```sh
pio test -e native -f 'test_littlefs_fileset'
```

Result: **27/27 passed**, exit 0. Seven new tests cover short writes, write EINTR, a seven-byte prefix followed by EIO, fsync failure, verification read failure, altered read-back and close error. They intentionally assert that false may follow partial or complete visible bytes. The close callback closes the real POSIX descriptor before returning synthetic EIO; it does not model an actual VFS descriptor leak. This remains host evidence only. No test_monthset rerun was necessary because IFileSet/MonthSet/LogStore contracts and MonthSet implementation were unchanged; test_littlefs_fileset itself already exercises MonthSet and LogStore integration.

Because the class declaration/source has a host-only conditional branch, the affected inert ESP32 adapter-link target was rebuilt once. Exact command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — passed, exit 0, 1,769.31 s. Pinned target: Espressif32 8.0.0, ESP32 Dev Module, Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12, esptool 5.4.0, Xtensa 14.2.0+20260121. Memory: 67,356/327,680 B RAM (20.6%); 934,976/1,310,720 B application flash (71.3%). Exact ELF command: ./tools/verify_v0.2.72.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf" — passed 12/12 NVS wrapper-routing checks and retained 9/9 LittleFsFileSet public methods, exit 0. Never flash the smoke image. No mounted LittleFS runtime, physical filesystem fault test, power cut or recovery behavior is established.

## v0.2.71 LittleFsFileSet target compatibility

The owning host suite and target compile/link evidence for the adapter are:
- pio test -e native -f 'test_littlefs_fileset'
- pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
- tools/verify_v0.2.71.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"

The host suite verifies POSIX lstat() no-follow behavior. The target branch uses ESP-IDF stat() because the pinned newlib does not declare lstat() and the LittleFS backend has no symlink entry type. The final ELF checker verifies all nine public LittleFsFileSet methods are retained and re-runs the 12 NVS wrapper-routing checks. This is compile/link and static-symbol evidence only: the smoke target performs no filesystem operation. Do not flash this image. C13 remains open until mounted-target VFS fault injection and an approved crash-recovery/power-cut protocol are verified.

## Widening coverage

- Add adjacent suites when a shared interface or invariant changes.
- Run the full native suite after changes to event formats, core storage/replay, config semantics, authentication/session state, cross-module ordering, or shared APIs used by many modules.
- Run target compile/link when ESP32 HAL code, vendor-library dependencies, linker flags, target configs or adapter composition change.
- Do not repeat a full suite after every small, isolated patch. Do not claim unrun registrations passed.

## Verification limits

Native tests prove host logic only. ESP32 compile/link proves compatibility with the compiled target source set. Final-ELF inspection proves only the inspected linked call paths. The NVS guard remains narrow: it blocks only an exact full DATA_NVS erase through esp_partition_erase_range; chunked and alternate erase paths are not covered.

The diagnostic probe is not attendance firmware. Never flash the inert esp32-adapter-link image. Do not deliberately corrupt NVS on the user's only device. Hardware claims require actual device output and observation.
