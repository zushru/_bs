# SF2 learner-identity review evidence — architecture review v0.2

Status: **design review only; no authentication, verifier, key store, persistence, API or UI has been implemented. All decisions in this document remain proposals until explicitly approved.** This review supersedes v0.1 where noted, but does not silently resolve any OPEN item in `docs/DECISIONS.md`.

## 1. D4 outcome

D4 is not ready for implementation. The v0.1 proposal correctly rejects `SOURCE_CHECKED` as evidence, but its signature and trust model was underspecified. In particular, `signerKeyId/signature` did not say whether the reviewer or the device signs; a device signature does not by itself prove the identity of the human reviewer. A signed event is also not trustworthy if the public-key registry, boot image, or committed record can be replaced without detection.

The safest current behavior remains unchanged:

- `SOURCE_CHECKED` is an untrusted administrative claim.
- `projectReportingRoster()` must keep identity status `UNVERIFIED` without authenticated evidence.
- D3 join remains `INCOMPLETE`.
- `calculateSnapshotMonth()` remains preview-only and must never authorize export.
- Do not persist or expose real learner PII.

## 2. Threat model must precede implementation

The product has not stated which adversaries the evidence mechanism must resist. At minimum, design review must distinguish:

| Threat | Existing protection/evidence | Current gap |
|---|---|---|
| Unauthenticated Wi-Fi client | WPA2 AP is part of the device setup; admin operations are intended to require tokens | No production HTTP/TLS/auth adapter; C6 is caller responsibility |
| Client who knows the shared admin PIN | Session token and PIN-lockout logic exist as pure core helpers | One shared admin credential cannot attribute an individual reviewer; callers can omit lockout/persist-before-verify |
| Malicious or compromised admin session | No reviewer-role system | Existing admin authority is too broad to prove a school review |
| Stolen reviewer phone/key | No reviewer credential system | No revocation, recovery or re-enrollment process |
| Physical flash extraction or replacement | No proven NVS encryption/secure-boot provisioning | C16/C27 unresolved; no trusted key registry or storage-integrity root |
| Power loss during identity/review commit | No reporting metadata store | C13/C26/C29 unresolved; no transaction or recovery semantics |
| RTC correction or invalid RTC | RTC is adjustable; no authenticated time source | Wall-clock time cannot prove when a review occurred |
| Modified browser/client | Browser models are reference code only | Client flags, calculated digests and `VERIFIED` strings are forgeable |

A design that only protects against an unauthenticated Wi-Fi client is materially weaker than one that also resists a malicious authenticated administrator or physical flash attacker. The acceptance target must be explicit. A cryptographic mechanism must not be described as tamper-proof if its trust anchor is stored in replaceable ordinary flash.

## 3. Separate human proof from device receipt

The evidence contract must distinguish two different statements:

1. **Reviewer attestation:** an individually authenticated reviewer confirms an exact identity revision and the explicitly selected fields against a declared source.
2. **Device commit receipt:** the device records that this attestation and the matching metadata revision were durably committed together.

A device signature can prove only the device-side receipt, assuming its key is trustworthy. It does not prove which human reviewed the data. A reviewer signature can bind the review to identity data, but does not prove the device committed it atomically or retained it without alteration. Do not collapse both into one ambiguous `signerKeyId/signature` pair.

Candidate logical separation (not an approved wire format):

- `reviewerAttestation`: reviewer principal, reviewer-key ID, exact student ID, identity record ID, metadata revision, reviewed-field set, source category/reference, decision, canonical identity binding, and reviewer signature.
- `deviceCommitReceipt`: committed metadata revision, attestation ID, commit sequence, storage transaction ID, and a device-authenticated receipt only if a trustworthy device key/root is selected.

The attendance event log must not be extended casually with LRN, legal names, source references or other PII. C25's current event validator rejects unsupported event types; a separate encrypted metadata/audit component is the safer logical boundary, subject to C26/C27/C29.

## 4. Reviewer identity and roles — unresolved

The existing six-digit admin PIN is a shared device credential. It must not be reused as an individual reviewer identity. A `reviewerId` supplied as free text, a browser session token, device name, or shared PIN-derived label is not individual authentication.

Two viable policy shapes require a user/school decision:

| Option | Behavior | Benefit | Cost/risk |
|---|---|---|---|
| A — one named reviewer | One individually authenticated reviewer may enter and confirm the same record | Lowest workflow burden; works where one authorized staff member handles the record | No separation of entry and approval; requires school acceptance |
| B — separate editor and reviewer | One account enters/corrects; a different reviewer confirms | Better separation of duties | Requires at least two trained users and a role/account lifecycle |

**Recommendation for consideration, not adopted:** use one named, individually authenticated `IDENTITY_REVIEWER` role for the first version; keep the existing shared admin credential separate. Do not require a second approver unless the school requires it. This avoids making a solo operator workflow impossible, but it is weaker than two-person control and must be accepted explicitly.

Each principal needs a stable ID, active/revoked status, authorization revision and credential-key binding. Revocation must invalidate future reviews and trigger an explicit policy for previously signed attestations. Do not infer role from possession of the device admin token.

## 5. Offline credential and trust-anchor options

No option is selected. A reviewer must be authenticated while the ESP32 may have no internet connection.

- A locally entered username/PIN is easy to implement but returns to password storage, brute-force, shared-device and transport problems; it does not automatically provide strong individual attribution.
- A reviewer-held asymmetric key can sign a device-bound challenge, while the device verifies against a provisioned public key. This avoids storing reviewer private keys on the ESP32, but requires an approved enrollment/revocation ceremony and a protected, integrity-anchored public-key registry.
- A phone-side hardware-backed key/native companion app may improve private-key protection, but adds an Android app and key-recovery lifecycle. A browser-only passkey/WebCrypto route on the device's local AP has not been validated for the actual Android browser, local HTTP origin, offline asset bundle or supported cryptographic APIs. **[UNVERIFIED: prove using the exact target phone/browser and deployed origin before selecting this path.]**

Do not implement a device private signing key in ordinary ESP32 flash and call it secure. The current project has no verified target build, secure element, NVS encryption, flash-encryption provisioning or secure-boot configuration. Public keys are not secret, but their integrity is security-critical: replacing the trusted reviewer-key registry can make an attacker-created key appear authorized. C16/C27 and the boot/storage trust model must be resolved first.

If the selected design cannot establish an integrity-protected reviewer-key registry and safe key enrollment/revocation, it must not transition records to `VERIFIED`. A weaker operational confirmation may still be recorded as an explicitly unverified claim, but it cannot satisfy the authoritative export gate.

## 6. Source evidence and identity binding — unresolved

The source category must be a controlled value accepted by the school. No source precedence or authoritative source list is inferred. Do not store photographs, scanned school records, passwords, unnecessary document numbers or unrestricted free-text evidence in the device. A minimal source category plus a carefully scoped locator may be enough, but even the locator can contain PII and must be reviewed before persistence.

A review must bind to the exact immutable identity version, not only a student ID. It must enumerate the fields explicitly confirmed (at minimum the legal-name components, LRN and sex if those fields are required in the report). An identity correction changes the binding and invalidates prior review for the changed data. `reviewedFields` must not permit partial confirmation to be mistaken for full identity approval.

The v0.1 `identityDigest` proposal is incomplete as an implementation contract. Before use, specify canonical field order, UTF-8 normalization policy, byte encoding, length framing, null/empty distinction, LRN preservation, extension-name handling and hash/signature algorithm. An unsalted hash of low-entropy PII is not anonymization and must not be exposed in public API responses, URLs, diagnostics or ordinary logs. Prefer signing the canonical identity payload inside the protected evidence component; keep any digest internal and never treat it as a substitute for confidentiality.

Source categories, evidence-reference policy, name normalization and school-approved fields remain OPEN pending school direction. The uploaded XLSX is not an authority for this policy.

## 7. Time and ordering — unresolved

The DS3231 value is not trusted time by itself. It can be manually adjusted, and there is no authenticated time source. Monotonic uptime orders events only within a boot and cannot establish a calendar date across resets. Source sequence orders committed records but does not prove real-world time.

Separate:

- **Ordering:** an immutable metadata revision/commit sequence and transaction ID.
- **Elapsed time:** a monotonic counter scoped to a boot/session, if needed.
- **Calendar timestamp:** an RTC or external timestamp plus explicit quality/provenance.

Recommendation for consideration: never use RTC time alone to authorize `VERIFIED` or to resolve valid-time school identity. Keep review validity bound to exact metadata revision and signed content; label the calendar timestamp `UNTRUSTED` unless an independently approved time-setting procedure establishes quality. Whether uncertain calendar time blocks an otherwise correctly bound review is a school/product decision and remains OPEN.

## 8. Commit, backup, restore and revocation requirements

A valid reviewer attestation and its metadata record must commit as one recoverable transaction. A browser `committed:true` field is not evidence. The device must be able to distinguish:

- no commit;
- complete metadata + review commit;
- interrupted/incomplete transaction;
- record present but signature/authorization invalid;
- identity corrected after review;
- reviewer later revoked;
- restored record whose review evidence or key-registry revision is missing.

The metadata revision, reviewer-key registry revision, attestation history and required audit state must be included in one versioned authenticated backup/restore generation. A restore must never report success if identity data survives but its review evidence, registry, or transaction journal does not. This is blocked by C13/C26/C27/C29 and the incomplete C2 recovery orchestrator.

The event/audit chain must have a defined genesis, canonical encoding, sequence rules, revocation semantics, backup root and verification path. A hash chain alone is not tamper-proof if an attacker can replace the entire chain and its root. A device signature is only meaningful if its private key and boot/storage trust are protected.

## 9. Required states and gate semantics

Keep the existing proposed states, but derive them only from verified facts:

- `UNREVIEWED`: no valid current attestation.
- `REVIEW_PENDING`: workflow opened; nothing committed or authorized yet.
- `VERIFIED`: the trusted verifier has validated an active individual reviewer credential, signature, reviewer role, exact record/revision binding, complete field coverage, source policy, audit sequence and required time-quality policy; the commit/backup model is also intact.
- `REJECTED`: authenticated reviewer rejected the exact version; report remains blocked until corrected and reviewed.
- `REVOKED`: a previously accepted attestation was explicitly revoked or its required authority was withdrawn.
- `INVALID`: malformed, mismatched, unauthorized, replayed, duplicate, chain-invalid or cryptographically invalid evidence.

No browser property, `SOURCE_CHECKED`, user-entered `reviewerId`, boolean `verified`, or schema `VALID` state may cause a transition to `VERIFIED`. Until the trusted verifier and durable commit path exist, no implementation path may produce that state.

## 10. D4 implementation gate

Do not implement the verifier or PII persistence until all of the following are resolved:

1. Threat model: remote-only, malicious authenticated admin, physical flash access, or another explicit boundary.
2. Reviewer policy: Option A or B above, authorized school role(s), account/key registration, revocation and recovery.
3. Credential path: offline phone/browser/native route demonstrated on the exact target environment.
4. Source policy: school-approved source categories and minimal evidence reference.
5. Time policy: what timestamp is required and what evidence makes it trusted.
6. Trust anchor: public-key registry integrity, device/boot integrity, and key provisioning/replacement/recovery.
7. Commit and recovery: atomic metadata+attestation transaction, audit verification, and C29 backup inclusion.
8. PII protection: at-rest/in-transit confidentiality, access control, no-store, redaction, and no PII in ordinary logs.

Until then, retain preview-only calculations and keep real learner PII out of firmware persistence and API responses.
