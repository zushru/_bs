# Desktop Commander project instructions

Use the existing connected Android device running Termux + Debian PRoot as the development environment when available. The device is slow; optimize for fewer expensive test/build cycles and avoid unnecessary verbose output.

## Before work

1. Confirm the connected device, current directory and exact project snapshot/version.
2. Read `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/DECISIONS.md`, and the latest release/audit notes.
3. Preserve the current source tree. Never silently replace a newer or modified tree with an older archive.
4. Treat `SIGNED`, `SIGNED-HANDOFF`, `DELEGATED`, `MEASURED`, `OPEN`, `DEFECT`, and `SUPERSEDED` exactly as documented. Do not make product-policy changes to unresolved decisions without the user's instruction.

## Efficient execution

- Default to targeted native suites. Select test filters from the files changed; do not run the full PlatformIO suite after every increment.
- Batch safe source-only increments before asking for input. Stop only at a real decision boundary or hardware-only proof requirement.
- Run only one CPU-heavy PlatformIO task at a time. Use `-j 2` for ESP32 builds.
- Keep normal output. Turn on verbose/debug flags only to diagnose a concrete failure.
- Do not continuously poll a quiet long-running job. Check CPU/process activity, then retrieve output in batches.
- Do not repeat checks that already passed on the exact same source fingerprint unless a later change invalidated them.
- Never upload/flash automatically. The inert link-smoke image must never be flashed. The diagnostic probe is separate and may be flashed only after review and explicit hardware-test authorization.

## Evidence discipline

For every result, distinguish:
- native tests (logic only);
- ESP32 compile/link (toolchain/API compatibility only);
- final-ELF inspection (linked routing only);
- device output (hardware behavior).

A successful build is not a successful device test. Do not report an assertion count as passed unless the final summary and exit code confirm it. Keep logs outside the release source tree.

## Delivery

Deliver a complete replacement tree, not partial file patches. Exclude `.git`, `.pio`, build caches, dependency directories and logs. Update the context, changelog, tests, audit, release notes and checksum manifest. Explain known blockers and unverified claims; do not mark a critical issue resolved merely because a smoke target links.
