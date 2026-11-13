# v0.2.89 — fail-closed scan metadata and directory enumeration

**Status: development-only. Focused native suites passed; full ESP32 target link and mounted-VFS runtime remain unverified.**

## Baseline and protected scope

- User replaced the entire `/root/att` tree before this increment. The extracted snapshot identified itself as v0.2.88; all 435 entries in its source manifest verified before edits. No Git metadata is present.
- `HANDOFF_FINAL.md` and `docs/DECISIONS.md` were not edited. Their current SHA-256 values are recorded in `docs/WORKING_MEMORY.md`.
- Source manifest is regenerated for this complete snapshot; the delivered archive excludes `.git`, `.pio` and transient caches.

## Defect and change

The previous `scan_()` called `ensureLogDirectory_(false)`, then repeated root and `/log` metadata lookups after it failed. A transient `stat()` error could disappear on the second lookup and be misclassified as a missing `/log` directory, returning success with a zero count. This is a data-integrity defect: callers could interpret an I/O error as an empty month set.

The new scan preflight inspects root and `/log` exactly once. Root metadata failure or a non-directory root fails. A failed `/log` lookup means valid-empty only when `errno == ENOENT`; all other errors fail. A present `/log` must be a directory. The scan then uses the existing native-only `statFn`, `opendirFn`, `readdirFn` and `closedirFn` callbacks, including regular month-file metadata. Counts and next-key outputs remain zero until the complete scan and `closedir()` succeed, so a late enumeration or close error cannot publish a partial count.

`ensureLogDirectory_()` also routes host metadata through the existing `statFn`. Under `ESP_PLATFORM`, these branches call the real target `stat()`, `opendir()`, `readdir()` and `closedir()` directly; the callback table is unavailable to product code. No public product API, persisted format, mount owner, recovery policy or destructive operation changed.

## New regressions

Six registrations were added to `test/test_littlefs_fileset/test_main.cpp`:

1. root `stat` returns `EIO` and must not become empty;
2. `/log` `stat` returns `EIO` and must not become empty;
3. month-file `stat` returns `EIO` without publishing a count;
4. `opendir` returns `EIO`;
5. `readdir` returns `EIO` after one actual month entry and the partial count is discarded;
6. `closedir` reports `EIO` and the count is not published.

Directory fault callbacks fail once so the follow-up byte-preservation checks can perform normal reads. The fake callbacks wrap real host POSIX directory handles; `closedir` closes the handle before synthesizing the reported error.

## Verification evidence

| Command | Result | Scope |
|---|---|---|
| `pio test -e native -f 'test_littlefs_fileset'` | **43/43 passed**, 17.210 s | Owning POSIX/native fileset suite, including six new scan regressions and earlier validator/append fault cases |
| `pio test -e native -f 'test_littlefs_mount_owner'` | **22/22 passed**, exit 0, 21.021 s | Directly dependent storage owner fail-closed behavior |
| `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link` | **INCOMPLETE / manually stopped** | Modified `littlefs_file_set.cpp` compiled and `liblittlefs_file_set.a` archived under the pinned Xtensa/ESP-IDF toolchain. The build continued rebuilding unrelated HAL objects; no final target link, `firmware.elf` or `firmware.bin` was produced. |

The two red-phase runs reproduced the intended missing-hook and transient-stat failures. PlatformIO emitted SIGILL/SIGQUIT/SIGINT after the Unity failure output; these are recorded as assertion-level red evidence, not clean red process exits. The final green run supersedes those failures for the owning suite.

Source inventory: **725 C++ `RUN_TEST` registrations across 39 suites**; `test_littlefs_fileset` has **43**. SF2 remains 270 Node registrations across 12 files and was not run. No full native N1, SF2 suite, final ELF inspection, hardware-probe build, flash, mounted LittleFS VFS operation, NVS test, power-cut/brownout test or physical-device test was performed.

## Logs retained on the development host

- `/tmp/att_v0289_scan_red.log`
- `/tmp/att_v0289_scan_red2.log`
- `/tmp/att_v0289_fileset_final.log`
- `/tmp/att_v0289_mount_owner.log`
- `/tmp/att_v0289_esp32_build.log`

## Remaining constraints

This is POSIX host error-propagation evidence plus a target translation-unit/library compile. It does not prove ESP-IDF LittleFS `errno`, `DIR*`, descriptor lifecycle, directory-entry durability, file-data durability, `fsync`/`close` behavior, low-space handling or restart semantics. The inert adapter-link target is never to be flashed. C13.3/U110 remain open until mounted-target VFS fault tests, a production storage owner/root, a proven commit primitive, restart-safe transaction/journal and physical interruption evidence exist. D1–D11 are unchanged; formatting, truncation, quarantine and repair remain disabled.

## Exact commands

```sh
pio test -e native -f 'test_littlefs_fileset'
pio test -e native -f 'test_littlefs_mount_owner'
pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link
sha256sum -c MANIFEST.sha256
```
