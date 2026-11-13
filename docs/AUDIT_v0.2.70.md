# Critical audit — v0.2.70 ESP32 smoke-link and BSS measurement

Date: 2026-11-10  
Baseline: v0.2.69

## Finding 1: the smoke target had drifted behind production interfaces

The smoke target still declared member-function pointer types for mutable results and private methods that had since been restricted:
- \`As608Runtime::attendance()\` and \`enrollment()\` are const member functions returning const references, not mutable non-const accessors.
- \`As608Runtime::occupancy()\` is a const member function returning \`const Occupancy&\`.
- \`occupancyReader()\`, \`enrollmentSensor()\` and \`enrollmentFsm()\` are private to the runtime's coordinator/pipeline friends.
- \`As608AttendanceClient::invalidateReady()\` is private to its runtime/startup friends.

The first target build failed with actual Xtensa compiler errors on these declarations. The correct repair was to bring the smoke anchors in line with the public const status API and remove the private-member anchors. Making the private production methods public would have weakened the ownership boundary and was explicitly not done.

The corrected source compiled and linked successfully. \`roster_snapshot.cpp\`, the AS608 core, the runtime/startup, and ESP32 adapters all compiled in the same pinned target build.

## Finding 2: roster audit footprint was previously only estimated

v0.2.69 added a per-student lifecycle-day field. The old release note estimated the field's array cost from \`301 * sizeof(int32_t)\`, but did not measure the complete audit object on the actual target ABI.

The smoke target now retains one static \`RosterTransitionAudit\`. The exact final Xtensa ELF reports:

\`\`\`text
3ffc7a60 00000e2c b setup()::rosterAuditSmoke
\`\`\`

The complete object is **3,628 bytes** (\`0xE2C\`) in BSS. This is the total object, including all fixed per-ID state and finding data. Do not mislabel 3,628 bytes as the incremental cost of the one new field. The field's nominal element storage is 1,204 bytes for 301 \`int32_t\` entries, but this increment did not build a v0.2.69 comparison ELF, so the exact ABI-level before/after delta is not directly measured.

PlatformIO reports the complete smoke target at 67,308 bytes RAM and 927,120 bytes application flash. These numbers include the broad linked smoke libraries and code, not a final production application.

## Build evidence and red-phase failure

First attempt:

\`\`\`sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
\`\`\`

Failed at compile time because the smoke target had stale method pointer declarations. This was a harness compile failure; it did not expose a defect in the production classes.

After updating only \`target/esp32_adapter_link/main.cpp\`, the same command succeeded (exit 0) with:
- Platform: Espressif32 8.0.0
- Board: \`esp32dev\`
- Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5
- Xtensa toolchain 14.2.0
- RAM 67,308 / 327,680 (20.5%)
- Flash 927,120 / 1,310,720 (70.7%)

No native test suite was rerun because no native behavior changed. The 43/43 v0.2.69 roster tests are historical and are not relabeled as v0.2.70 verification.

## Finding 3: direct ELF inspector command was unusable in the release workflow

\`tools/verify_v0.2.51.sh links\` invokes \`tools/inspect_probe_wrappers.sh\` directly. The inspector had mode 0640, so direct invocation returned \`Permission denied\`. Invoking it through \`bash\` worked, but that did not repair the documented workflow.

The executable bit is now 0755. The exact direct invocation against the final smoke ELF passed 12/12 assertions and printed \`NVS_WRAPPER_ROUTING=PASS\`.

This still does not prove runtime NVS safety. The guard permits smaller range requests; alternate or direct erase pathways are not comprehensively blocked. C34 remains unresolved.

## Remaining risks

- This is only the inert adapter smoke target. No hardware-probe target was built.
- U59 remains open for production task stack, heap, fragmentation, maximum replay latency and observer-budget measurements.
- U61 remains open: the semantic audit still does not govern every operational roster writer/consumer. \`RosterStore::historyComplete()\` remains a parser/projection integrity predicate.
- C3/C6/C13 and U112 remain unresolved.
- No target was flashed and no physical device behavior is claimed.
- E18, E27 and all other delegated decisions remain unchanged.

\`HANDOFF_FINAL.md\` and \`docs/DECISIONS.md\` remain unchanged.
