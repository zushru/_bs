# AUDIT v0.2.92 — C3 application attendance gate

## Scope

Critical review of the new ApplicationAttendanceGate, its integration into As608AttendanceOwner and As608OwnerConfigRuntime, its test fixture and its inert ESP32 link anchors. The audit does not approve a production root or close C3.

## Positive evidence

1. **Arbitrary admission is removed from the preferred owner API.** As608AttendanceOwner now requires ApplicationAttendanceGate; the compile-time type assertions reject the old generic IAttendanceAdmission constructor.
2. **The gate binds a coherent in-memory dependency graph.** It checks the exact READY storage owner's IFileSet through MonthSet and LogStore, the exact attached RosterStore and LogAttendanceAdmission observers, and the ready/non-pending ClosureEngine with exact log/roster references.
3. **Startup and attendance are distinguished.** H_SENSOR_FAULT can permit bounded read-only sensor initialization only when the remaining config/storage/clock/replay graph is sound. Attendance and commit require H_OK. The pipeline checks admission again at match commit.
4. **Config identity is checked.** Owner construction compares the supplied Config to ConfigBootCoordinator's loaded Config and checks roster UTC offset, admission Params and supported closure schedule fields.
5. **Config runtime identity is checked before persistence.** A runtime bound to another ClosureEngine refuses canApply and applyAfterCommit. The regression proves no config slot write occurs in the mismatched case.
6. **The NEEDS_SETUP fixture respects boot ordering.** Storage owner allocation and all dependent runtime graph allocation are deferred until ConfigBootCoordinator reports READY. The NEEDS_SETUP denial test uses an already-built independent dependency graph only to call the gate with a non-ready coordinator; it does not build runtime services from the NEEDS_SETUP coordinator.
7. **Evidence is focused and attributable.** The owning native suite passes 16/16. The inert target compile/link passes and the final ELF symbol table contains the gate, owner, closure-check and config-runtime methods.

## Residual risks / non-claims

- There is still no production application root or call site. A type-level requirement on the preferred owner does not prevent unrelated code from constructing low-level clients, runtimes or other owner objects outside that composition boundary.
- No singleton, mutex or cross-task protection is added. All gate checks and dependent state changes rely on the documented serialized application-loop contract.
- The gate observes HealthFsm state; it does not initialize the DS3231, read the RTC, set/validate time, acquire the shared I2C bus or prove that health inputs reflect physical state. The production root/clock caller must supply truthful serialized updates.
- The gate observes LittleFsStorageOwner READY and graph identity; it does not exercise the mounted ESP-IDF VFS. Host POSIX/fake mount tests cannot establish durability, filesystem commit semantics, low-space handling, interruption behavior or power-cut recovery.
- ClosureEngine config comparison covers the fields it owns (UTC offset, late/scan lead, timetable and termStart). It deliberately does not invent termEnd semantics; C18 remains unresolved.
- Comparing the supplied config with the boot-loaded config prevents startup with a copied/mismatched config, but does not implement schedule history, roster scratch replay/rebuild for UTC-offset changes or a safe live change of the immutable RosterStore offset.
- The gate does not resolve E18 or U5. No enrollment API was exposed and no fake UART page number was promoted to a physical page rule.
- H_OK correctness depends on the future production HealthFsm integration. A stale or incorrectly updated HealthFsm could still provide a false readiness result; this increment does not add a source of truth for health.
- The new smoke anchors are link-only. They force method retention but do not construct the gate/owner/runtime or invoke their behavior. The smoke image must never be flashed.
- C13.3, C1/C34, C6 and C2 are independent blockers and are not closed by this work.

## Test-first outcome

The red regressions reproduced config mismatch accepted at owner startup and a wrong closure instance accepted by the config runtime. The final focused suite passed 16/16. No other native suite or SF2 test was run.

## Disposition

The application gate is a useful C3 prerequisite: it narrows the preferred owner API and makes several composition errors fail closed. It is not an application root, scheduler, hardware-health implementation or physical attendance proof. Keep C3 BLOCKING.
