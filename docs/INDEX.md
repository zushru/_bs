# Documentation index and authority map

## Read order

For every agent session, first batch-read the root context pack in order: AGENTS.md, WORKING_MEMORY.md, index.md and AI_COORDINATION.md. Then use this document for detailed documentation navigation. Historical archives are not first-read context.

## Authority and current state

| File | What it controls | What it does not do |
|---|---|---|
| `../HANDOFF_FINAL.md` | Original signed product handoff and accepted baseline | Does not include every later decision change. |
| `DECISIONS.md` | Signed, delegated, open, measured, defective and superseded decisions | Does not prove implementation. |
| `../ARCHITECTURE.md` | Source-oriented component relationships and integration state | Does not override module contracts. |
| `CRITICAL.md` | Active blockers and their consequences | Not a substitute for technical evidence. |
| `UNVERIFIED.md` | Claims requiring more proof | Not a general TODO list. |
| `DEVELOPMENT_ROADMAP.md` | Approved engineering sequence and gates | Does not authorize changing decision status. |
| `ONE_DAY_EXECUTION.md` | Time-boxed graduation/demo strategy | Does not downgrade production requirements. |
| `TESTS.md` | Verification ledger and inventory | Registration count is not a pass count. |
| `TESTING_WORKFLOW.md` | Efficient test/build commands and evidence classes | Does not make unrun commands verified. |
| `NO_TEST_ASSURANCE.md` | Static-only assurance protocol for periods when execution testing is unavailable | Does not replace tests or authorize production release. |
| `PACKAGE_AUDIT_v0.2.95.md` | Packaging provenance, preserved hashes and exact verification performed | Not a firmware release or hardware audit. |
| `CHANGELOG.md` | Chronological change history | Not a current specification. |
| `ADAPTER_DESIGN.md`, `BRINGUP.md` | Detailed adapter design and historical hardware observations | Not proof of current physical operation. |

## Module specifications

- `SPECS/INDEX.md` maps each module contract to source and likely owning tests.
- `SPECS/` contains 21 module specifications. Inspect the relevant contract before editing.

## SF2 workstream

- `sf2/INDEX.md` is the SF2 navigation and status index.
- `sf2/README.md`, `REQUIREMENTS.md`, `DATA_CONTRACT.md` and `D4_DECISIONS.md` describe the reference-model and policy boundary.
- Versioned design documents describe proposals/models; version number alone does not confer approval.
- `sf2/TEACHER_QUESTIONS.md` records unresolved school-specific rules.
- `sf2/reference/` contains reference-only supplied workbooks, not approved final templates.

## Historical material

`archive/README.md` explains the archive. `archive/releases/` and `archive/audits/firmware/` retain earlier increments. `archive/context/` retains superseded context files. `archive/patches/` is provenance only; do not rerun archived patch scripts against this baseline. `../tools/legacy/` contains version-specific verification tools, not current general verification commands.
