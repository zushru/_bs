# Specialized SF2 audit — v0.2.26

Date: 2026-11-07  
Scope: D4 reviewer-evidence architecture review, starting from the independently checksum-verified v0.2.25 archive.

## 1. Release disposition

**Documentation-only development snapshot.** No C++ or JavaScript source, test registration, attendance event, product rule, key material, metadata store, authentication path, API, dashboard or workbook writer changed. D4 is reviewed but deliberately not implemented because its trust and reviewer-policy inputs are OPEN.

## 2. Source-of-truth and starting integrity

- Started from `/mnt/data/att_v0.2.25_2026-11-07.tar.gz`.
- The provided sidecar passed `sha256sum -c` before extraction.
- Read `docs/CONTEXT.md` and `docs/CRITICAL.md`, then `docs/DECISIONS.md`, `docs/UNVERIFIED.md`, the SF2 next-step plan, D3 join contract, snapshot coordinator design and D4 reviewer-evidence proposal.
- No repository or user tree outside this archive was assumed.

## 3. Critical D4 findings

1. **Signer ambiguity:** v0.2.1 used one `signerKeyId/signature` pair without separating the human review statement from the device's durable-commit receipt. Device signature does not establish the human reviewer; reviewer signature does not prove the device committed the record atomically. v0.2.2 separates the two concepts explicitly.
2. **Shared credential mismatch:** the signed product model has one shared six-digit administrator PIN. It cannot establish individual reviewer identity. A user-supplied `reviewerId` or session token would be attribution text, not proof.
3. **No trust root:** the current project has no verified target build, secure element, NVS/flash-encryption provisioning or secure-boot provisioning. A reviewer public-key registry stored in ordinary replaceable flash can itself be substituted. A device signing key in ordinary flash must not be described as secure.
4. **Offline browser path unproven:** the local AP browser flow has not been proven to support the required credential API and cryptographic operations on the actual phone/browser/origin. A native companion app would materially change scope. This is recorded as unverified, not asserted impossible.
5. **RTC is not trusted time:** the DS3231 is adjustable and no authenticated time source exists. Monotonic uptime does not prove a calendar date across reboots. Ordering, elapsed-time measurement and calendar time must be separate fields.
6. **Evidence-source policy missing:** no school-approved source taxonomy or reference-minimization policy exists. The archive/template cannot supply that school decision.
7. **Digest leakage risk:** an unsalted digest of low-entropy PII is not anonymization. Do not expose identity digests in URLs, diagnostics, public endpoints or normal logs.
8. **Commit/backup mismatch:** metadata review and identity would need a recoverable atomic commit and one backup/restore generation. C13/C26/C27/C29/C2 block that work.
9. **Hash-chain overclaim risk:** a hash chain without a protected root can be replaced as a whole. A device signature is only useful when its key and boot/storage trust are protected.

## 4. Changes in v0.2.26

- Added `REVIEW_EVIDENCE_DESIGN_v0.2.md`, correcting the ambiguous signer model and setting out the threat model, reviewer identity, credential, key-registry, source-evidence, time, digest, commit, backup and verification requirements.
- Added `D4_DECISIONS.md` with OPEN choices and recommendations. No recommendation has been adopted as a product decision.
- Updated `NEXT_STEPS.md`, `CONTEXT.md`, `README.md`, release notes and changelog to record D4 as reviewed but blocked pending explicit decisions.
- Recorded new implementation uncertainties U75-U78. No status in `docs/DECISIONS.md` was changed. E27 remains DELEGATED and unchanged.

## 5. Verification and test accounting

- No production code changed; no functional test registrations were added or run.
- C++ registrations remain **376 across 23 suites**: 318/318 was user-reported for v0.2.17 only; the later 58 registrations remain unrun.
- SF2 Node registrations remain **256**, all authored but unrun.
- The prior 25-module `node --check` result remains syntax-only evidence for v0.2.25; it is not a functional test and was not rerun because no `.mjs` changed.
- No C++ compilation, PlatformIO, ESP32 target build, device test, cryptographic test, browser credential test, storage/power-loss test, HTTP test, XLSX round-trip or print test was performed.

## 6. Remaining blockers

- D4-1 through D4-8 are OPEN; see `D4_DECISIONS.md`.
- C6/C13/C16/C26-C29/C2 still block authentication, PII persistence, trusted snapshot delivery and backup/recovery.
- C17/E2 and C18/E26 block reliable historical schedule/term completeness.
- C19/C20/C23 and U62-U74 still block final official workbook/calculation acceptance.
- D3 remains a client/reference-model boundary, not a device-side security boundary. Browser state remains preview-only.

## 7. Next action

Resolve the threat model (D4-1) and reviewer separation policy (D4-2) first. Then select and prove an offline credential path on the exact Android phone/browser and define the trust anchor. Do not implement a verifier, PII store or API until all D4 gates and C13/C16/C26-C29 are resolved.

## 8. Archive integrity

The release archive is independently extracted and every regenerated `MANIFEST.sha256` entry is checked. `.git/`, `.pio/`, `node_modules/`, Python caches and general build caches are excluded. The archive SHA-256 sidecar is generated from the final packaged bytes and checked.
