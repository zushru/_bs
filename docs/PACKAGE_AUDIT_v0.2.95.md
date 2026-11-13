# Package reconstruction audit — v0.2.95

**Prepared:** 2026-11-12  
**Purpose:** document how the complete development/context ZIP was assembled from the uploaded `att.zip` and matching v0.2.95 tar snapshot. This is a packaging audit, not a firmware release or hardware audit.

## Inputs and provenance

- Uploaded `att.zip`: 207 files.
- Matching tar snapshot: 452 files under `att/`.
- All 207 ZIP files exist in the tar snapshot and have identical SHA-256 contents. The ZIP's source/test/model files were retained byte-for-byte.
- The tar snapshot's original 451-entry `MANIFEST.sha256` passed before reorganization.
- Protected files were not edited:
  - `HANDOFF_FINAL.md`: `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`
  - `docs/DECISIONS.md`: `52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3`

## Reorganization

- 59 older release notes moved to `docs/archive/releases/`; v0.2.95 remains at repository root.
- 59 older firmware audits moved to `docs/archive/audits/firmware/`; v0.2.95 remains active in `docs/`.
- Superseded working memory, context, critical and unverified documents preserved under `docs/archive/context/`.
- Historical patch artifacts moved to `docs/archive/patches/` and marked as provenance only.
- 30 version-specific verifier scripts moved to `tools/legacy/`; current inspection utilities remain under `tools/`.
- 15 SF2 development audits moved to `docs/sf2/archive/audits/`.
- Older reviewer-evidence design revisions v0.1–v0.5 moved to `docs/sf2/archive/design-revisions/`; v0.6 remains as the latest retained design, not an implementation.
- Historical SF2 next-step notes preserved under `docs/sf2/archive/context/`.
- New active AI context, architecture, module indexes, blocker/evidence registers and one-day execution guidance added.

## Source changes

No firmware C/C++ source, native test source, SF2 JavaScript source, fixture or package definition was changed by this preparation. The original 207 files were rechecked across `lib/`, `test/`, `web/` and `platformio.ini`; all matched the uploaded ZIP. The active documentation was rewritten/reorganized where it contained stale or duplicated context. Shell script execute bits were enabled so included shell utilities can be run directly where the environment permits.

## Verification performed during packaging

- `cd web/sf2 && npm test` — **270/270 passed**, Node v22.16.0, exit 0.
- `node --check` — passed for SF2 source modules, fixture module(s) and all 12 test modules.
- Active Markdown link audit — no missing local links in the active documentation entry points checked.
- Original protected file hashes confirmed unchanged.

## Not performed

- PlatformIO was unavailable in the packaging environment; no native C++ suite or ESP32 target was rerun during package preparation.
- The v0.2.95 100/100 focused native result is retained as the release's recorded prior result, not represented as a packaging-pass rerun.
- No actual ESP32, AS608, RTC, OLED, Wi-Fi, LittleFS mount/runtime, power-cut, brownout or production end-to-end test was performed.
- No production firmware root/API/dashboard/XLSX exporter was created by this documentation/package task.

The package's newly generated `MANIFEST.sha256` covers every packaged file except itself. The ZIP's external SHA-256 is supplied beside the archive.
