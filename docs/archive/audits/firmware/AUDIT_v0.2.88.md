# Critical audit — v0.2.88

## Scope

This increment adds only native test-only syscall indirection around the `validateRecordFiles()` read-only preflight. It extends the existing injection seam for `pread` and `close` with `lstat`-equivalent path metadata, read-only file `open`, descriptor `fstat`, and directory `opendir`/`readdir`/`closedir` operations. The real POSIX APIs remain the default callbacks. Directory callbacks operate on real handles created by `opendir`; no fake `DIR` implementation or replacement enumeration model was added.

The `ESP_PLATFORM` branch remains direct system calls. The added seam is not a production abstraction. `scan_()`, `countChecked()`, `monthKeyChecked()`, and ordinary IFileSet metadata/enumeration paths do not use these new hooks.

## Fault cases added (not executed)

1. `/log` metadata returns `EIO` with expected file count zero: validator must not report empty/healthy.
2. A month file's metadata lookup returns `EIO`.
3. Read-only month-file `open()` returns `EIO`.
4. `fstat()` returns `EIO`; validator must release the opened descriptor and fail.
5. `opendir()` returns `EIO`.
6. `readdir()` returns `EIO`; validator must attempt to close the directory handle and fail.
7. `closedir()` reports `EIO`; validation must fail even after all entries were observed.

The byte-preservation cases compare the original one-record file after failure. The existing `pread` and descriptor-close regressions from v0.2.86/v0.2.87 remain pending too. **None of these tests was executed in this increment.** The user's request deferred test execution until RDC is available; no test-first red result or green result is claimed.

## Adversarial review

- `statFn` preserves host `lstat()` no-follow behavior when the callback is not faulting. This does not prove ESP LittleFS metadata behavior, and the injection only affects validator call sites.
- `readdir()`'s `errno` is cleared before every call, so a null return with `EIO` is distinguished from end-of-directory. The callback test injects an error before returning an entry; runtime ordering on the target is untested.
- The close callbacks release the real host descriptor/directory handle before returning synthetic `EIO`. This intentionally avoids leaking test resources, but cannot model a real close failure that leaves an OS/VFS resource uncertain.
- `scan_()` is still not injectable. A separate, carefully bounded seam would be needed to test `countChecked()`/ordinary metadata paths without implying target coverage.
- There is no target-side check that LittleFS/newlib has identical `errno`, directory-iteration, file-handle, or close behavior under fault conditions.
- No production composition root wires the owner; caller serialization remains a contract rather than a device-wide mutex/singleton. No storage durability primitive, journal, repair transaction, restart replay, partition-full behavior, or power interruption was proven.

## Build evidence

| Check | Result | What it proves |
|---|---|---|
| All production library C++ translation units | 57/57 compiled, exit 0 | Host compilation only |
| Changed host storage TU with `-Werror` | Passed, exit 0 | Strict native branch compilation |
| `-DESP_PLATFORM` storage TU with host compiler and `-Werror` | Passed, exit 0 | Conditional branch syntax against host headers only; not target compilation |
| Test source with temporary Unity shim and `-Werror` | Passed, exit 0 | C++ syntax only; no Unity runtime or assertions |
| Core + LittleFS host link closure | 32 objects linked, exit 0 | Host symbol/link closure, not firmware |

### Exact commands and include roots

The 57-unit build iterated over sorted `lib/**/*.cpp` sources with this compiler/include configuration:

```sh
INCLUDES=(
  -Ilib/core/src -Ilib/as608/src -Ilib/hal_esp32/src -Ilib/littlefs_file_set/src
  -Itest/test_as608_gate/stubs -Itest/test_nvs_config/stubs
  -Itest/test_esp32_secure_random/stubs -Itest/test_esp32_wifi_ap/stubs
  -Itest/test_esp32_oled/stubs
)
while IFS= read -r src; do
  rel=${src//\//_}
  out="/tmp/att_v088_host_objects/${rel%.cpp}.o"
  g++ -std=gnu++11 -Wall -Wextra "${INCLUDES[@]}" -c "$src" -o "$out"
done < <(find lib -type f -name '*.cpp' | sort)
```

Strict native and host-preprocessed target-branch checks used the same `INCLUDES` array:

```sh
g++ -std=gnu++11 -Wall -Wextra -Werror "${INCLUDES[@]}" \
  -c lib/littlefs_file_set/src/littlefs_file_set.cpp \
  -o /tmp/att_v088_host_objects/littlefs_file_set_strict.o

g++ -std=gnu++11 -Wall -Wextra -Werror -DESP_PLATFORM "${INCLUDES[@]}" \
  -c lib/littlefs_file_set/src/littlefs_file_set.cpp \
  -o /tmp/att_v088_host_objects/littlefs_file_set_esp_branch_syntax.o
```

The owning test TU syntax check used the temporary, non-Unity shim at `/tmp/att_v088_unity_syntax/unity.h`:

```sh
g++ -std=gnu++11 -Wall -Wextra -Werror \
  -I/tmp/att_v088_unity_syntax "${INCLUDES[@]}" \
  -c test/test_littlefs_fileset/test_main.cpp \
  -o /tmp/att_v088_test_littlefs_fileset_syntax.o
```

The host link closure used 32 `lib/core/src` and `lib/littlefs_file_set/src` objects:

```sh
g++ -std=gnu++11 -Wall -Wextra -pthread \
  /tmp/att_v086_host_link_main.cpp \
  $(find /tmp/att_v088_host_objects -maxdepth 1 -type f \
    \( -name 'lib_core_src_*.o' -o -name 'lib_littlefs_file_set_src_*.o' \) | sort) \
  -o /tmp/att_v088_storage_link_probe
```

These commands were executed successfully in the local host environment. The temporary shim and dummy link `main()` are outside the project bundle. No PlatformIO test, SF2 test, ESP32 target link/ELF, flash, VFS runtime or physical test was run.

## Disposition

The change improves host fault observability in the read-only validator but does not close C13.3 or U110. Keep anomalous storage read-only. Formatting, truncation, quarantine, repair, and recovery remain disabled. No signed or delegated decisions changed.
