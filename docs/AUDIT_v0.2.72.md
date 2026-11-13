# Critical audit — v0.2.72

## Increment under review

C13.1: introduce host-only fault injection for LittleFsFileSet appendFile() and verify the existing uncertain-commit contract without implementing recovery.

## Baseline and authority

- Root: /root/att
- Baseline: v0.2.71 archive, SHA-256 6986c72183fe6b3142f2d65bd970ba32f191ee5013887d957ee7f846bf2461ea
- Protected HANDOFF_FINAL.md and docs/DECISIONS.md hashes matched their recorded baseline.
- Checkout has no Git metadata. Archive extraction comparison passed, excluding .pio.
- No signed or delegated product rule was changed.

## Design review

The callback seam is:
- Compiled into the class interface/source only when ESP_PLATFORM is not defined.
- Per instance, not global mutable test state.
- Optional; the ordinary native constructor passes null and retains direct POSIX calls.
- Limited to appendFile write, fsync, verification pread and close.
- Not included from ESP32 builds; the test header deliberately emits a compile error if ESP_PLATFORM is defined.

The public ESP32 constructor and append syscall path remain direct. The target build is required because conditional class members and constructors differ by platform. A passing target build only checks the firmware branch compiles and links; it does not exercise the host seam or LittleFS runtime.

## Regression quality

Seven new tests cover:
- short write progress;
- write EINTR retry;
- partial write followed by EIO;
- fsync error after all bytes were written;
- verification read error after all bytes were written;
- verification mismatch;
- close error after the file descriptor was closed by the test wrapper.

The assertions inspect persisted bytes after failures. This is materially stronger than only asserting a false return because it proves the outcome is indeterminate.

The close test is intentionally limited: callback closes the POSIX fd, then returns synthetic EIO. It cannot prove how ESP-IDF VFS behaves when close itself genuinely fails, nor whether a descriptor remains allocated. A future target-specific descriptor-lifecycle test remains required.

The short-write test uses a callback that accepts at most five bytes per call. The EINTR test injects one interrupted call then permits the real write. The hard-write-error test accepts seven bytes then returns EIO.

## Verification evidence

- Before callback wiring, direct native test executable: 27 tests, 7 expected failures, 20 passes, exit 7. No SIGBUS.
- The corresponding PlatformIO wrapper emitted SIGBUS after the expected nonzero child exit and printed an inconsistent case count. This is recorded as a runner anomaly, not evidence that the assertions themselves crashed. Preserve the logs for future diagnosis.
- After callback wiring: pio test -e native -f 'test_littlefs_fileset' — 27/27 passed, exit 0.
- Target command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — passed, exit 0, 1,769.31 s. Environment: Espressif32 8.0.0 / ESP32 Dev Module; Arduino-ESP32 3.3.12; ESP-IDF libs 3.3.12; esptool 5.4.0; Xtensa 14.2.0+20260121. Memory: 67,356/327,680 B RAM (20.6%); 934,976/1,310,720 B app flash (71.3%).
- Exact ELF command: ./tools/verify_v0.2.72.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf" — passed 12/12 NVS wrapper-routing checks and retained 9/9 LittleFsFileSet public methods, exit 0. This is compile/link/static evidence only; no target runtime operation occurred.
- No test_monthset rerun: this patch changes only LittleFsFileSet's native append syscall injection seam; it does not change IFileSet, MonthSet or LogStore contracts. The owning test file already includes MonthSet/LogStore rollover/reopen and torn-tail fail-closed integration coverage.
- No full native suite, SF2 suite, probe build, flash, physical-device, LittleFS mount/runtime, brownout or power-cut test.

## Failure modes not resolved

1. Real VFS write can fail after writing a prefix. This remains possible and now has POSIX regression evidence only.
2. fsync/read-back/close can fail after the complete bytes are visible. Caller must continue treating false as uncertain.
3. LogStore latches after append failure, but partial records can still prevent future open because truncation is disabled.
4. No crash-safe quarantine, truncate or replace protocol exists.
5. No production mount owner, serialized storage owner or production firmware composition exists.
6. Host POSIX filesystem behavior is not ESP-IDF LittleFS behavior.
7. No evidence yet for filesystem-full, wear, directory sync, metadata ordering, power-cut or brownout behavior.

## Risk disposition

C13 remains blocking. The increment improves observability and test coverage without changing the failure contract or enabling destructive actions. It does not make storage production-ready.

Next: finish the one affected ESP32 build, inspect the exact final ELF, finalize the manifest/archive only after all documentation is settled, and then proceed to C13.2 recovery protocol design. No recovery implementation should be enabled until the protocol and its interruption matrix are reviewed and specifically approved.
