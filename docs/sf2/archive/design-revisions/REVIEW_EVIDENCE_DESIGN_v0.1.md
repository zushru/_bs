# SF2 learner-identity review evidence — design v0.1

Status: **architecture proposal only; not approved, implemented, persisted, or cryptographically verified**.

## 1. Purpose and hard boundary

The D2 metadata model accepts `SOURCE_CHECKED` as a caller claim. It does not prove who checked the record, what source was checked, when the check occurred, or which exact identity revision was checked. D3 keeps the report blocked for this reason.

The review event must attest to a specific immutable identity version, not merely to a student ID or a mutable "verified" flag. A change to legal name, LRN, sex, or the metadata revision invalidates the old attestation for the changed content.

This proposal does not choose a persistence backend, encryption method, account system, device key, or source-document policy. C6, C13, C16, C26, C27 and C29 remain blockers.

## 2. Proposed logical record

A future append-only `IDENTITY_REVIEW` event could contain:

| Field | Requirement |
|---|---|
| `attestationId` | Unique immutable event ID |
| `studentId` | Existing never-reused device ID, 1..300; never LRN |
| `identityRecordId` | Exact bitemporal identity record being reviewed |
| `metadataRevision` | Exact committed metadata revision containing that record |
| `identityDigest` | SHA-256 over a documented canonical encoding of first/middle/last/extension name, LRN and sex; excludes display labels |
| `reviewedFields` | Explicit field set; no inference that an unchecked field was reviewed |
| `reviewerId` | Stable individual reviewer principal, not a shared PIN, token, device label or free-text name |
| `sourceKind` | Controlled source category approved by the school; no assumed LIS/source precedence |
| `sourceReference` | Minimal non-secret reference; do not store a scan/photo or unnecessary document number |
| `decision` | `CONFIRMED`, `REJECTED`, or `REVOKED`; later correction is a new event, never mutation |
| `recordedAt` | UTC timestamp plus explicit clock-quality state; a raw RTC value is not trusted time by itself |
| `previousAuditDigest` | Prior chain digest or explicit genesis value |
| `signerKeyId`, `signature` | Device/server signature over the canonical event, identity digest and prior audit digest |

The table describes a candidate logical contract only. Field encodings, required source kinds, reviewer roles, signature scheme and retention rules are OPEN.

## 3. Verification rules

1. The reviewer must be authenticated as an individual. The current shared admin PIN/token cannot establish individual attribution. A free-text reviewer name is not authentication.
2. The attestation must bind the exact `studentId`, identity record ID, metadata revision and canonical identity digest. A review for an older revision cannot verify a corrected identity.
3. The reviewer must explicitly confirm each required field. A name check does not implicitly confirm LRN or sex.
4. The source category and reference must identify the evidence used without copying unnecessary student documents into the device.
5. A trusted verification component must validate the signature, chain linkage, reviewer authorization and metadata binding. Browser JSON flags such as `verified:true`, `committed:true` or `SOURCE_CHECKED` are not proof.
6. Revocation, identity correction, reviewer deauthorization, failed signature validation, broken audit chain, or uncertain time quality makes the affected report incomplete. It must not silently preserve a prior `VERIFIED` label.
7. A new metadata revision may retain an attestation only when the canonical reviewed identity digest is unchanged and the audit policy explicitly allows the binding. The safe default is to require a new review after identity changes.

## 4. Security and implementation dependencies

- **C6:** Individual reviewer identity requires caller-side authentication and authorization, session expiry, lockout and persist-before-success enforcement. A shared administrator secret cannot distinguish two staff members.
- **C16/C27:** Signature or MAC keys need a provisioning, confidentiality, rotation, recovery and compromise plan. The current NVS configuration is not proven encrypted. Do not place a long-term signing secret in ordinary flash and call it secure.
- **C13/C26:** The review event must be committed atomically with the metadata transaction or linked through a recoverable transaction journal. A browser-side `committed` flag is insufficient.
- **C29:** Review evidence must be included in the same authenticated backup/restore generation as the identity record it attests. A restored identity without its evidence is unverified.
- **C25:** The event stream needs a current semantic validator and replay/recovery integration if evidence is stored in an append-only device stream. The attendance log must not be casually extended with PII event types.
- **Clock integrity:** The RTC may be adjusted and has no proven authenticated time source. A monotonic event sequence can order events but cannot prove a real-world review date. Time quality must be explicit.

## 5. Readiness states

Proposed states:

- `UNREVIEWED`: no attestation exists for the current identity digest.
- `REVIEW_PENDING`: a review workflow is started but no valid committed evidence exists.
- `VERIFIED`: a trusted verifier has checked signature, reviewer authorization, chain, record binding and field coverage.
- `REJECTED`: an authenticated reviewer rejected the identity; report remains blocked until corrected and reviewed.
- `REVOKED`: previously accepted evidence was revoked or its binding is no longer valid.
- `INVALID`: evidence is malformed, forged, replayed, mismatched or chain-invalid.

Only `VERIFIED` may satisfy the identity-review gate. The current v0.2.25 code does not implement any transition to `VERIFIED` and must remain `INCOMPLETE`.

## 6. Decisions intentionally not made

- Which staff roles may enter or approve identity metadata.
- Whether one reviewer can enter and confirm their own changes or a second reviewer is required.
- Which official school source is authoritative for legal name, LRN and sex.
- Whether source references are free-form, controlled, or represented by a digest.
- How the device authenticates individuals while offline and connected only through its local AP.
- Whether a device-held signature key is acceptable without secure-element or hardware-backed key protection.
- Trusted-time policy when the DS3231 has been adjusted or reports uncertain time.

Until these are resolved and security/storage dependencies are implemented, do not persist or expose real learner PII and do not call a report authoritative.
