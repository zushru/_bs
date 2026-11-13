# One-day graduation execution plan

**Purpose:** maximize the chance of delivering a defensible, demonstrable result within one day without damaging attendance-data integrity or making false claims.  
**Baseline:** v0.2.95.  
**Hard truth:** the supplied repository does not contain an integrated production root/API/dashboard, and C13.3/C6/C2 remain open. Full production completion in 24 hours is not a credible promise from this baseline.

## 1. Define what “finished” means before coding

Within the first 30–45 minutes, write down the actual graduation acceptance criteria. Separate them into:

1. **Must physically work:** what the evaluator will watch on the real ESP32, sensor, display, RTC, Wi-Fi and storage.
2. **Must be demonstrated in software:** core attendance rules, roster behavior, report calculations or UI behavior.
3. **Must be documented:** architecture, limitations, testing evidence and future work.

Do not assume all three are interchangeable. A passing unit test is not a physical fingerprint attendance demonstration. A UI mock is not a device API. A compile/link target is not a running attendance system.

If the rubric requires the complete live path (fingerprint → correct student identity → attendance commit → durable reboot/replay → authenticated display/report), this baseline does not yet prove that path. Do not claim it does. If the user has a separate integrated application or newer hardware-tested tree, that tree must be supplied and verified before it can become the development baseline.

## 2. First-hour go/no-go gate

- Make a protected copy of the current known-good ZIP. Do not overwrite the only baseline.
- Extract to a clean directory and run `sha256sum -c MANIFEST.sha256`.
- Check the actual PlatformIO/Node environment, serial port, board, sensor, RTC, OLED, power supply and available test time.
- Read `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, `docs/CRITICAL.md` and the exact demo rubric.
- Confirm whether the actual physical sensor has existing diagnostic templates. The legacy sketch can leave a duplicate template; do not perform blind cleanup.
- Decide whether the demo is a real device workflow, a bounded hardware diagnostic plus software-evidence presentation, or a reference-model demonstration. Label it honestly.

**Stop condition:** if the rubric requires end-to-end production attendance and the necessary root/hardware path cannot be composed and safely verified, do not spend the day pretending the gap can be hidden. Switch to the strongest truthful demonstrable slice and explicitly list what is incomplete.

## 3. Priority order

### P0 — protect the baseline (first)

- Preserve the original ZIP and tar separately.
- Verify the new manifest before edits.
- Never edit `HANDOFF_FINAL.md` or `docs/DECISIONS.md` to make tests/demo easier.
- Do not flash `esp32-adapter-link`; it is inert compile/link code.
- Do not format storage, erase NVS, alter partitions, delete sensor templates or enable recovery shortcuts.

### P1 — establish a repeatable software baseline

- Run the SF2 suite if SF2 is in the demo: `cd web/sf2 && npm test`.
- Run the relevant native test suite(s) for the feature being demonstrated, not a random subset.
- If the current native environment is stable and time allows, run `pio test -e native` once at a meaningful checkpoint; do not run it after every minor edit.
- Keep each result's exact command, full log, count and exit code.

### P2 — choose one vertical slice

A vertical slice must have a real start, observable result and honest failure behavior. Select the path that is already closest to being operational and supported by actual available hardware. Do not try to finish attendance, storage recovery, restore, full auth/API, dashboard and authoritative SF2 export simultaneously.

Candidate paths, subject to physical availability and rubric:

- **Hardware diagnostic slice:** prove the selected hardware components respond using the separate probe after reviewing its code and wiring. This is a diagnostic demonstration only; it is not attendance functionality.
- **Attendance-logic slice:** demonstrate deterministic core attendance/admission behavior through the relevant native tests and a controlled fixture. State that this is host evidence, not a real sensor/device workflow.
- **SF2 reference slice:** demonstrate the reference calculations and tests with controlled fixtures. State that it is not production API data or an approved XLSX export.
- **Integrated device slice:** attempt only if an actual production root or separately verified integration already exists. The supplied v0.2.95 archive does not contain one. A rushed root that bypasses health, storage, authentication or idempotency is worse than a clearly bounded demo.

Do not use fabricated students, synthetic event logs or mock screenshots as evidence of physical operation. Fixtures are acceptable only when clearly labeled as fixtures.

## 4. Suggested 24-hour time budget

| Elapsed time | Deliverable | Cut rule |
|---|---|---|
| 0:00–0:45 | Rubric, hardware inventory, protected backup, manifest verified | Do not code before the required demonstration is clear. |
| 0:45–1:30 | Choose one vertical slice; define 3–5 observable acceptance checks | If no safe demo path exists, choose a truthful diagnostic/reference demonstration. |
| 1:30–5:30 | Implement only the smallest missing piece for that slice | No architecture-wide refactor or new persistence format. Stop if it threatens the baseline or needs unresolved product decisions. |
| 5:30–9:00 | Owning regression tests and dependent tests | Fix regressions; do not hide failures or loosen assertions. |
| 9:00–13:00 | Target build and controlled hardware validation, if hardware is available | No flashing of smoke target; no destructive recovery or sensor erase. |
| 13:00–16:00 | Repeat the exact demo from a clean boot/setup state; capture logs and limitations | If it cannot repeat twice, it is not a reliable demo. |
| 16:00–19:00 | Freeze features; fix only show-stopper defects | No new features after this point. |
| 19:00–22:00 | Prepare presentation/evidence: architecture, acceptance checklist, test logs, known limits | Never present host tests as hardware proof. |
| 22:00–24:00 | Final clean build/test, archive/checksum, restore-point backup and contingency | Preserve a known-good deliverable even if a last-minute change fails. |

This is a time-allocation guide, not a promise that any missing integration can be completed in the listed block.

## 5. Multi-agent operating protocol

- Use AI_START.md with the assigned codename at the start of each fresh agent session.
- Provide access to the latest accepted canonical repository snapshot. Do not assume ChatGPT accounts, OpenCode, Claude, ZIP uploads or RDC workspaces synchronize automatically.
- Give one task with explicit acceptance criteria. Ask for exact files to inspect before allowing broad changes.
- Prefer this loop: inspect → one small patch → focused test → review diff → checkpoint.
- Do not ask for repeated whole-repository summaries. The indexes and working memory already provide navigation.
- Ask the model to return a small patch or updated complete ZIP and to list every changed file. Keep your own known-good local copy.
- At context/usage interruption, the current owner records the exact baseline, changed paths, verification, unresolved work and next owner in AI_COORDINATION.md and WORKING_MEMORY.md. Release the ownership lock only after writes have stopped and the tree has been read back; the next agent bootstraps from the four context files.
- Never assume free-tier quotas, model context capacity, file access or persistence across chats. Save every accepted artifact locally.

## 6. Definition of a defensible demo

A demo is defensible only when all applicable items below are true:

- The exact tested archive/version is identified.
- The displayed behavior corresponds to actual code or clearly labeled test fixtures.
- The same demonstration succeeds twice from a known reset/start condition.
- No attendance event is duplicated after an uncertain storage result.
- Missing coverage/data is not presented as absence.
- No unsafe shortcuts bypass authentication, health or storage gates.
- The person presenting can explain what is real, simulated, host-tested, target-built and physically verified.
- Known limitations are written down and disclosed.

## 7. Do not spend the final day on these

Unless the graduation rubric explicitly requires one, defer: transactional restore; automatic destructive storage recovery; universal flash-erase protection; new persistence formats; reporting PII persistence; authoritative reviewer evidence; final XLSX writer and print fidelity; general refactoring; broad cosmetic cleanup; new features with unresolved policy inputs.

These are not unimportant. They are unsafe to compress into a deadline-driven shortcut.

## 8. Final handoff checklist

- [ ] A known-good pre-change ZIP is preserved.
- [ ] The final project extracts cleanly and its manifest passes.
- [ ] The exact demo scenario and acceptance checks are documented.
- [ ] All changed code has focused regression evidence.
- [ ] The applicable full suite/build result is recorded, or explicitly marked not run.
- [ ] Hardware checks identify the exact board/peripherals and observable outcomes.
- [ ] No smoke target is described as production firmware or flashed.
- [ ] Limitations and unresolved blockers are disclosed.
- [ ] A final checkpoint and external ZIP checksum are saved.
