# SHRU critique inbox

The canonical repository is /root/prototype/att. PISHDA is the sole
implementation owner and repository writer. SHRU is an optional read-only
critic; its only write permission is this directory.

## Write a critique report

SHRU may create or revise one Markdown report per requested critique:

comms/inbox/SHRU/<TASK-ID>.md

No source/test/documentation changes, staging files, diffs, pre-images, build
artifacts or other writes are permitted for SHRU. Do not create reports in
QYUPAL or SEI directories; those roles are retired. Historical inbox folders
and reports, if present, are retained as provenance only.

## Required report header

~~~md
# <TASK-ID> — <short title>

- Author: SHRU
- Status: COMPLETE | BLOCKED | PARTIAL
- Baseline: <version / snapshot identity>
- Workspace: CANONICAL_SHARED_TREE | ISOLATED_COPY | UNKNOWN
- Inspected: <exact paths>
- Not inspected: <important omissions>

## Findings
- [SEVERITY] Finding — exact path and symbol/section; evidence and impact.

## Required actions
- ...

## Verification
- Commands/tests run: NONE
- Not run: tests, compilers, builds, PlatformIO, flashing, hardware
~~~

## Rules

1. SHRU may read repository material but may write only inside
   comms/inbox/SHRU/.
2. SHRU must not run project commands, tests, compilers, builds, PlatformIO,
   flashing or hardware actions.
3. State whether the inspected files came from the canonical shared tree or
   an isolated copy. Do not claim delivery to PISHDA from an isolated copy.
4. Cite exact files, symbols and line numbers where stable. Separate confirmed
   issues from plausible risks and claims that cannot be proven without
   execution.
5. Do not include secrets, credentials, student personal data or large copied
   source excerpts. Preserve prior findings when revising a report.
6. Inbox presence means received, not accepted. PISHDA verifies each finding
   and records any resulting action in AI_COORDINATION.md.
7. SHRU feedback is supplemental. PISHDA does not wait for a report or depend
   on it for implementation progress. There is no automatic notification or
   background process.

## Current optional critique

- ATT-NO-TEST-ASSURANCE-REVIEW-01.md
