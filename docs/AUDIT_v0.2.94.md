# v0.2.94 audit — LittleFS month-file creation fault handling

Date: 2026-11-12
Scope: LittleFsFileSet::addFile() and its native-only fault seam.

## Observed change

The host test interface now allows openFn to receive the mode_t argument used by open(). readFile()'s host helper supplies mode zero for O_RDONLY; addFile() supplies 0600 for O_WRONLY | O_CREAT | O_EXCL. Native addFile() routes open, fsync and close through the per-instance hook table. The ESP_PLATFORM branch remains direct.

No production syscall abstraction was introduced. The test header remains guarded against ESP_PLATFORM.

## Failure semantics

| Injected fault | Observed adapter result | File-state implication |
|---|---|---|
| open returns EIO | addFile() returns false | No file created by this call |
| fsync returns EIO | addFile() returns false | Empty file already exists; it is not removed |
| close closes descriptor then returns EIO | addFile() returns false | Empty file exists; close callback simulates an error after releasing the host descriptor |

The fsync and close tests deliberately confirm the created file remains enumerable with size zero and that a blind same-key retry is rejected. This is the safe adapter-level behavior: do not infer “no side effect” from false and do not delete evidence to make retry succeed.

## Verification

- Red reproduction: all three new creation-path assertions failed before hook routing. The expected failures were observed in the live RDC test output; no separate red log file was retained.
- Green:
  pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'
  - fileset 50/50
  - mount owner 23/23
  - serialized fileset 3/3
  - total 76/76, exit 0
  - log /tmp/att_v0294_c13_native.log
- Target:
  pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
  - success, 1,987.27 s
  - RAM 67,892 B (20.7%); flash 1,010,340 B (77.1%)
  - ELF SHA-256 18af01b5664e794b22f27e2d3acb383dbbbdfb3c00135a4a08d3198cfb677660
  - BIN SHA-256 40ce2e4416b2a59c524bf40d97b7d648566b0054d894e9aa7cdbcd88a689c909
  - log /tmp/att_v0294_esp32_link.log
- ELF inspection confirmed fileset create/read/append/validator methods and the mount-owner/mount-backend symbols remain linked. This is not an execution trace.

## Critical review

1. Create may have happened on failure. fsync or close failure follows a successful exclusive create. The file is not rolled back. Any caller treating false as “no file” can dead-end on retry or misclassify the fileset.
2. No safe rollback exists here. The implementation intentionally does not unlink the file, because that would be a new destructive cleanup policy without a proven commit point.
3. No directory durability is proven. fsync(fd) is on the newly created file; this change adds no parent-directory sync and does not prove that the name survives power loss.
4. The host close shim is synthetic. It closes the actual POSIX descriptor and then returns EIO. It tests propagation only, not the pinned ESP-IDF VFS descriptor lifecycle.
5. Low-space behavior is untested. The injected open failure is generic EIO, not a real LittleFS-full condition. No near-full partition was exercised.
6. No production adoption. The adapter and owner are not integrated into a production composition root or scheduler.

## Disposition

C13.3 remains BLOCKING. These changes add host fault coverage and improve evidence honesty; they do not close mounted-target VFS, low-space, directory-entry durability, restart, interruption, or physical power-cut requirements.

No truncateFile, quarantineCopy, quarantineFrom, MonthSet::repair, format, recovery journal, or storage-swap behavior was enabled. D1–D11 and protected decision/handoff files are unchanged. The inert esp32-adapter-link target was not flashed.
