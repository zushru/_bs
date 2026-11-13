# Release v0.2.70 — ESP32 smoke-link and roster-audit footprint verification

Date: 2026-11-10  
Baseline: v0.2.69  
Project root: /root/att

## Increment scope

This increment repairs and extends the inert ESP32 adapter smoke application. It does not alter production core behavior or introduce a production firmware root.

### Changes

1. Updated \`target/esp32_adapter_link/main.cpp\` to use the current const/read-only public accessors on \`As608Runtime\`. Removed stale link anchors that named private friend-only accessors (\`occupancyReader\`, \`enrollmentSensor\`, \`enrollmentFsm\`, \`invalidateReady\`) and old mutable-return signatures. No runtime API was made public and no access control was weakened.
2. Retained an inert \`RosterTransitionAudit\` object and its public \`consume()\` symbol in the smoke target. This ensures the actual Xtensa ABI/link includes the audit class and provides an exact BSS symbol size.
3. Updated the smoke target README to describe the additional audit link/footprint coverage.
4. Changed \`tools/inspect_probe_wrappers.sh\` from mode 0640 to 0755. The existing \`verify_v0.2.51.sh links\` workflow invokes this script directly; before this fix it stopped with \`Permission denied\`.

No production firmware API, event format, persistence format, product behavior, handoff, or decision status changed. \`HANDOFF_FINAL.md\` and \`docs/DECISIONS.md\` are unchanged.

## Build evidence

Exact command:

\`\`\`sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
\`\`\`

Final run: **SUCCESS**, exit 0.

Pinned target details reported by PlatformIO:
- Board: Espressif ESP32 Dev Module (\`esp32dev\`)
- Platform: Espressif32 8.0.0
- Arduino-ESP32: 3.3.12 (\`4.30312.261024\`)
- ESP-IDF: 5.5.5
- Xtensa toolchain: 14.2.0
- Adafruit SSD1306 2.5.17, GFX 1.12.6, BusIO 1.17.4

Final PlatformIO size report:
- RAM: **67,308 / 327,680 bytes (20.5%)**
- Application flash region: **927,120 / 1,310,720 bytes (70.7%)**

The exact final ELF symbol measured with \`xtensa-esp-elf-nm -S -C --size-sort\`:

\`\`\`text
3ffc7a60 00000e2c b setup()::rosterAuditSmoke
\`\`\`

\`0xE2C\` = **3,628 bytes**. This is the complete retained \`RosterTransitionAudit\` object, not just the new \`lastLifecycleDay\` field. It is a target static-footprint observation from the inert smoke composition, not a measurement of a future production application or its task stacks/heap.

The first build attempt failed at compile time because several old smoke link anchors no longer matched the tightened runtime API. The diagnostic identified both const-return mismatches and references to now-private accessors. Those anchors were corrected to the current public read-only API or removed. The successful final build is a fresh final-source build after those corrections. Production APIs were not weakened to satisfy the smoke target.

## Final ELF checks

Exact command:

\`\`\`sh
tools/inspect_probe_wrappers.sh "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
\`\`\`

Result: **12/12 checks passed**, \`NVS_WRAPPER_ROUTING=PASS\`, exit 0.

The checks cover the two wrapper definitions, the \`initArduino()\` call sites, wrapper delegation to the real IDF functions, and routing of the three standard NVS bulk-erase APIs through the wrapped partition-erase boundary. This proves only the inspected linked call paths. C34 remains blocking because smaller/chunked erase ranges and alternate/direct flash paths are not fully covered, and no runtime/device validation exists.

The script initially could not be launched directly because it lacked its executable bit. After \`chmod 755\`, direct invocation passed. The prior \`bash tools/inspect_probe_wrappers.sh ...\` invocation also passed, but direct invocation is the required workflow behavior fixed by this release.

## Test evidence and what was not run

No C++ source behavior or native test was changed. The current inventory remains **621 C++ RUN_TEST registrations across 35 suites**.

The last focused native evidence remains the v0.2.69 result: \`test_roster_snapshot\` 26/26 and \`test_month_roster\` 17/17. These suites were **not rerun** in v0.2.70. The other 578 registrations were not run.

No SF2 Node tests, \`.mjs\` syntax suite, \`esp32-hardware-probe\` build, ELF for the hardware probe, flash, serial interaction, sensor, Wi-Fi, OLED, RTC, power-cut, brownout, power-bank, or physical-device test was performed.

## Critical limits

- The smoke target is inert and must never be flashed as attendance firmware.
- The measured 3,628-byte audit object and 67,308-byte smoke RAM total do not prove production composition fit. Production task stacks, heap high-water, fragmentation, replay CPU latency and final observer use remain unmeasured (U59).
- No production firmware main/scheduler, HTTP adapter/routes, authentication boundary, or production enrollment service was added (C3/C6).
- Roster semantic audit remains a reporting-prefix helper; it is not integrated at all operational roster write/consumer boundaries (U61). E27 remains delegated and unchanged.
- Storage recovery/truncate/quarantine and power-loss durability remain blocked (C13). U112 durable request idempotency remains unresolved.
- The target build does not prove physical pin behavior, sensor protocol behavior, AP first-beacon security, DHCP, panel rendering, clock retention, powerbank runtime or brownout handling.

## Complete replacement archive

The release archive is a complete project replacement, excluding \`.git\`, \`.pio\`, caches and compiled build outputs. Validate its SHA-256 sidecar and \`MANIFEST.sha256\` before replacing the existing tree.
