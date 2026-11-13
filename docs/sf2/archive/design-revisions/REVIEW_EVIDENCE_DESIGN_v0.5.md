# SF2 learner-identity review evidence — architecture v0.5

Status: **design only.** This revision incorporates the user's D4-2 decision from 2026-11-07: one authorized administrator may both enter and confirm a learner identity. This is self-review, not independent verification. D4-1 and D4-2 are resolved; D4-3 is DELEGATED to assistant judgment; D4-4, D4-5, D4-7 and D4-8 remain OPEN. No reviewer evidence transition, authentication adapter, persistence, API or UI has been implemented. D4-3 selects role-only attribution without any operator-supplied identity label.

## 1. Approved threat boundary

The intended network boundary is the signed handoff's WPA2 AP passphrase plus administrator PIN/session token:

- A client without the AP passphrase must not associate with the device AP.
- A client on the AP without a valid admin session may access only the intentionally public, minimal `GET /status` response.
- Protected API routes require the existing 128-bit RAM-only session token, with 15-minute idle expiry and at most two sessions.
- Login, setup-code and old-PIN checks must use the existing `PinLockout` contract, including persist-before-verify and the old-PIN requirement for PIN changes.

The signed handoff explicitly accepts plain HTTP and the risk that AP-passphrase disclosure may expose an admin token to an observer. This remains an accepted residual risk. No TLS, physical-flash protection, secure boot, flash encryption, anti-rollback, device signature, tamper resistance, or non-repudiation claim is introduced.

Out of scope: physical access/flash extraction or modification; malicious or compromised authenticated administrators; AP-passphrase/PIN/token compromise; RF jamming and denial of service. If the user later expands the threat model, this decision must be revisited explicitly.

See `NETWORK_SECURITY_BOUNDARY_v0.1.md` for the full adapter acceptance contract. The HTTP/JSON adapter and its tests do not exist; C6 remains a blocker. U79 also remains open: the configured passphrase has a shape check, not a strength check.

## 2. What a review record can claim

Under this threat model, a successfully authenticated administrator action can authorize a review operation. It does not independently identify a human if the administrator PIN is shared. Therefore:

- A typed reviewer name or ID is an operator-supplied claim, not cryptographic identity proof.
- Do not describe the shared admin PIN as an individual reviewer credential.
- Do not add an asymmetric reviewer key, device signing key, secure element, or public-key registry for the selected threat model.
- The user has approved one authorized administrator entering and confirming the same learner identity. Any such review must be explicitly labelled **self-review**; it must not be called independent verification or two-person approval.
- The shared PIN cannot distinguish multiple people who know it.
- **D4-3 is DELEGATED (assistant choice, not user-signed):** store only `actorRole=ADMIN` and `reviewMode=SELF_REVIEW`. Do not collect or persist a typed operator name, ID, initials or other self-attested identity label. Such a label cannot be authenticated under the shared-credential model, adds personal data, and can create a false impression of accountability.
- `actorRole=ADMIN` identifies the authorization principal only. It does not identify the human operating the phone, prove that a particular staff member acted, or create non-repudiation. A valid admin session is the network authorization gate, subject to the accepted R11 threat boundary.

**D4-2 is SIGNED.** This is a product workflow permission only. It does not prove who physically operated the phone, establish a distinct reviewer identity, or settle school source/time policy.

## 3. Reviewer attestation versus commit receipt

Keep these as separate logical statements, even without cryptographic signatures:

1. **Review action:** an authenticated administrator confirms a specific immutable learner-identity revision, identifies exactly which required fields were checked, and records a school-approved source category/reference.
2. **Commit receipt:** the device reports the metadata revision, transaction identifier and commit sequence only after the storage layer's defined write/read-back procedure succeeds.

The second statement is not a cryptographic signature and does not resist physical modification. It must not claim to prove more than the storage transaction actually demonstrated. C13/C26/C29 still block production metadata persistence and recovery.

No final wire format is approved. Candidate fields may include a review record ID, device learner ID, identity-record ID, exact metadata revision, reviewed-field set, decision, school-approved source category and limited evidence locator, `actorRole=ADMIN`, `reviewMode=SELF_REVIEW`, and source commit sequence. Do not store an operator name/ID/label, transient session token, PIN, or raw authentication material. These candidate fields do not authorize implementation; C6/C13/C26/C29 and the remaining D4 gates still apply.

## 4. School source evidence — OPEN (D4-4)

The source category list and evidence-locator policy must come from the school. Do not infer precedence from the supplied XLSX. Do not store photographs, document scans, credentials, unnecessary document numbers or unrestricted free text. A `SOURCE_CHECKED` string remains a claim until the authorized review action and required source fields are actually recorded and validated.

A review must bind to one immutable identity revision, not merely a learner ID. It must enumerate all required reviewed fields (legal-name components, LRN and sex if required by the report). Partial review must never be interpreted as complete review. The exact required field set and source categories remain OPEN pending school direction.

## 5. Time and ordering — OPEN (D4-5)

The DS3231 is adjustable and is not trusted calendar time. Keep these concepts separate:

- **Source order:** persistent metadata revision/commit sequence; this is the preferred deterministic ordering.
- **Monotonic elapsed time:** useful within a boot only; it does not establish calendar time across resets.
- **RTC timestamp:** a calendar-time observation that must be labelled untrusted until a school-approved time-setting/evidence procedure exists.

Recommendation: bind review validity to exact identity revision and source sequence; retain RTC time as advisory only. This remains a recommendation, not a signed rule.

## 6. Revision and revocation — OPEN (D4-7)

A correction to any reviewed field must create a new identity revision. A prior review must not silently carry over. Recommendation: invalidate review for the changed revision and require a new review; preserve the old review record for historical reconstruction. School acceptance of retroactive invalidation/report handling remains open.

## 7. Storage and backup — OPEN (D4-8)

Keep learner reporting metadata outside the attendance event log. Its store needs versioning, transactional commit, bounded compaction/migration, exact revision snapshots and power-loss recovery. C13 and C26 remain blockers.

**O8 is SIGNED:** backup = config + roster + log. Reporting metadata is not currently included. Do not silently expand the backup. If reports must remain reproducible after restore, the user must explicitly amend O8 or choose a separate approved export/recovery process. Until then, a restored device may lack historical reporting identity; authoritative SF2 export must fail closed for those months.

## 8. Implementation gates

Before real learner PII is persisted or exposed, resolve all applicable gates:

- D4-3: resolved as DELEGATED role-only attribution (`ADMIN` + `SELF_REVIEW`); this does not identify a human;
- D4-4: school-approved source categories and minimal references;
- D4-5: review-time and ordering semantics;
- D4-7: revision invalidation and historical report behavior;
- C6: actual HTTP adapter enforcement of route auth, PIN lockout, persist-before-verify and old-PIN checks;
- C13/C26/C29: durable storage, migration, backup and restore/recovery;
- C28: bounded snapshot service that cannot starve the serialized ESP32 loop;
- C31: server-side source construction and review gate; browser-supplied `verified` values remain untrusted.

D3 remains `INCOMPLETE`; `calculateSnapshotMonth()` remains `PREVIEW_READY` with `authoritativeExportReady:false`. No PII API, persistence or XLSX export is authorized by this document.
