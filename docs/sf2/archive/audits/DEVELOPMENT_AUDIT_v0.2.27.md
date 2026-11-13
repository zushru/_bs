# Specialized SF2/security audit — v0.2.27

Date: 2026-11-07  
Scope: incorporate the user's network threat-model decision into the D4 reviewer-evidence architecture. Source: checksum-verified v0.2.26 archive.

## 1. Release disposition

**Documentation-only architecture update.** No C++/JavaScript production source, test registration, attendance event, runtime behavior, credential implementation, persistent store, HTTP route, dashboard, or workbook writer changed. This release does not improve runtime security by itself.

## 2. User decision captured

D4-1 is now user-resolved: protect only against unauthorized Wi-Fi clients. Physical access/flash extraction or modification and malicious authenticated administrators are out of scope. The signed handoff's WPA2 AP + admin PIN/token approach and explicitly accepted plain-HTTP/AP-passphrase-disclosure risk remain unchanged.

The release records this in R11 of `docs/DECISIONS.md` and aligns D4 documents and critical-issue status with it. No other D4 policy item was silently approved.

## 3. Main architectural conclusions

1. A device signing key, reviewer public-key registry, secure element, secure boot, or flash encryption is not required for the selected remote-only threat model. No anti-tamper or non-repudiation claim is made.
2. A durable metadata transaction/revision/sequence receipt is still required, but it is not a cryptographic signature. C13/C26/C29 remain blockers.
3. The shared six-digit admin PIN plus RAM-only token can authorize a trusted administrator action under the selected threat boundary. It cannot identify a unique human. D4-2/D4-3 remain open.
4. A typed reviewer name/ID is a claim, not independently authenticated individual identity.
5. The DS3231 remains untrusted as a review timestamp. D4-5 remains open.
6. School-approved source categories/references remain unknown. D4-4 remains open.
7. Every reviewed identity must bind to an exact immutable metadata revision; revocation/report behavior remains open under D4-7.
8. O8 remains signed: backup = config + roster + log. Reporting metadata cannot be added to that backup without a separate user decision. C29 therefore remains a substantive blocker for recoverable authoritative SF2 metadata.

## 4. Network adapter contract review

`NETWORK_SECURITY_BOUNDARY_v0.1.md` captures the future HTTP adapter requirements. The following are requirements only; none is yet implemented:

- `GET /status` is the only public route and must disclose only basic, non-sensitive health/status.
- Every other protected route requires a valid 128-bit RAM-only token; setup-gated routes also require the setup code.
- Route authentication precedes health/permission details.
- Every login/setup-code/old-PIN attempt passes through `PinLockout`; persist the incremented counter before secret comparison and persist a reset before issuing a session after success.
- PIN change requires the current PIN.
- Tokens expire after 900 seconds idle and at most two are active.
- No PII in logs, URLs, diagnostic output or cache; PII responses require authenticated access and `Cache-Control: no-store`.
- Keep the AP client limit at two and preserve USB-only firmware/no OTA.

### Critical caveats

- C6 remains a production blocker: the HTTP adapter does not exist and caller obligations are not enforced.
- The config code checks AP passphrase shape (8–63 printable ASCII bytes), not entropy. U79 records that high-entropy/non-default passphrase provisioning has not been demonstrated.
- Plain HTTP is intentional per the signed handoff. If the AP passphrase leaks, session-token sniffing remains an accepted residual risk. Do not claim otherwise.
- A 128-bit token is only as sound as its random-byte source. The source implementation and target behavior are not present or verified.

## 5. Changed scope

- Added `NETWORK_SECURITY_BOUNDARY_v0.1.md`.
- Added `REVIEW_EVIDENCE_DESIGN_v0.3.md`.
- Updated `D4_DECISIONS.md`, `NEXT_STEPS.md`, `docs/DECISIONS.md`, `docs/CRITICAL.md`, `docs/UNVERIFIED.md`, `docs/CONTEXT.md`, `README.md`, `RELEASE.md`, `docs/CHANGELOG.md`, and the SF2 README/current identity-projection notes.
- Preserved v0.2.26 and earlier audit/design files as historical records.
- `HANDOFF_FINAL.md`, C++ source, JavaScript source, tests, sketches and product runtime behavior are unchanged.

## 6. Verification and test accounting

- Documentation-only review; no C++ or Node functional tests run.
- C++ source registrations remain **376 across 23 suites**. Only the v0.2.17 baseline of 318/318 is user-reported as passing; the later 58 registrations remain unrun.
- SF2 Node registrations remain **256**, all unrun.
- The prior Node `--check` syntax check is unchanged evidence for the same 25 modules; this release changed no `.mjs` file.
- No PlatformIO, C++ compilation, ESP32 build, AP/network test, Android/browser test, HTTP authorization test, PII persistence test, backup/recovery test, or hardware test was performed.

## 7. Remaining blockers

- D4-2/D4-3: whether one administrator may enter and confirm, and how reviewer attribution is represented honestly.
- D4-4: school-approved identity source categories and evidence locator rules.
- D4-5: trustworthy review-time semantics; DS3231 alone is not trusted.
- D4-7: identity-revision invalidation and historical report behavior.
- C6/U75/U79: production HTTP adapter, lockout persistence, token source, AP passphrase strength/provisioning and unauthorized-client tests.
- C13/C26/C29: metadata durability, recovery and backup; O8 must be explicitly amended before metadata can be included in the backup.
- C28: bounded snapshot service and loop-latency proof.
- C31: server-side review gate; browser flags remain untrusted.
- Official SF2 template acceptance, teacher-dependent calculations, dashboard integration and XLSX generation remain incomplete.

## 8. Next action

Resolve D4-2 (one named reviewer versus separate editor/reviewer). Do not implement a reviewer-verification transition or expose real learner PII before that choice and the applicable source/time/backup gates are resolved. Separately, C6 can be developed as a host-testable API security coordinator, but the actual adapter still requires exact library/API verification and target testing.
