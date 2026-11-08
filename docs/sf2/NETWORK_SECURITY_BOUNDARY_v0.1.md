# Network security boundary — v0.1

Status: **D4-1 resolved by the user on 2026-11-07.** The requested protection target is unauthorized Wi-Fi clients only. This narrows the adversary model; it does not approve reviewer policy, school evidence categories, PII persistence, or an export-ready state.

## 1. In-scope threat

Protect the device's Wi-Fi/API surface from a client that does not have the approved AP access credential and has not obtained a valid administrator session. The design relies on two existing layers:

1. **AP admission:** WPA2 access-point passphrase. The handoff says the passphrase is known only to the administrator and limits the AP to two clients.
2. **Application authorization:** the six-digit administrator PIN establishes a session; protected API routes require a 128-bit RAM-only token with a 15-minute idle timeout and a maximum of two sessions. `GET /status` is the deliberate exception and may expose only basic non-sensitive health/status information.

The current config contract only checks the Wi-Fi passphrase shape (8–63 printable ASCII bytes); that is not a strength test. A high-entropy, non-default AP passphrase is necessary for this boundary, but a generation/rotation ceremony has not been approved or verified. See U79.

The core decision layer already models route authorization, monotonic lockout timing and session-token checking. That is not proof that the future HTTP adapter enforces the contract. C6 remains a production blocker.

## 2. Explicitly accepted residual risks / out of scope

The signed handoff already accepts plain HTTP on the WPA2 AP and explicitly accepts that disclosure of the AP passphrase can permit an observer to sniff an administrator token. This boundary preserves that choice; it does **not** upgrade the product to TLS or claim confidentiality after AP-passphrase disclosure.

The requested threat model does not include:

- physical access to the ESP32, flash extraction/replacement, or firmware replacement;
- a malicious or compromised authenticated administrator;
- compromise/disclosure of the AP passphrase, administrator PIN, or a valid session token;
- radio jamming, denial of service, or attacks requiring privileged control of the phone/browser.

No secure-boot, flash-encryption, anti-rollback, tamper-resistance, non-repudiation, or device-signature claim is made. A cryptographic device signing key and reviewer public-key registry are not justified by this threat boundary.

## 3. Required API behavior before PII endpoints

These are adapter requirements, not currently implemented production behavior:

- Permit unauthenticated `GET /status` only, and return no learner identity, LRN, attendance detail, secrets, tokens, or security-sensitive diagnostics.
- Require a valid session token for every other protected API endpoint. Setup-code-gated routes must require both the appropriate setup code and the required route authorization.
- Apply route authentication before health/permission details so a tokenless client learns no protected state.
- Route every login, setup-code check, and old-PIN check through `PinLockout`; check lockout first and durably persist the incremented failure counter **before** comparing the submitted secret. If verification succeeds, clear and persist the counter before issuing a session or completing the PIN change; persistence failure must fail closed.
- Require the current PIN for a PIN change. A valid session token alone must not change the PIN.
- Generate session tokens from a cryptographically secure random source; reject RNG failure. Tokens remain RAM-only, 128-bit, limited to two sessions, and expire after 900 seconds idle.
- Preserve the two-client AP limit. Do not add remote/cloud access or OTA.
- Add adapter-level tests for route matrix, lockout ordering/persistence, old-PIN enforcement, token expiry/revocation, public-status redaction, malformed headers, and concurrent clients.
- Keep PII out of request logs, response errors, URLs, and diagnostic output. PII responses require `Cache-Control: no-store` and explicit authentication.

v0.2.31 adds a portable `PinAuthCoordinator` for the ordering of lockout persistence, secret verification, counter reset and protected action. It also routes PIN changes through current-PIN verification. This is a reusable core primitive, not production enforcement: the HTTP server, JSON adapter, concrete durable counter store, PBKDF2 verifier/writer, secure-random adapter and session/fault integration are still absent. The above remains an acceptance contract until every secret route is wired through the coordinator and tested on the exact ESP32/browser stack.

## 4. Consequences for D4 reviewer evidence

The network admin session can authorize an administrator action under this threat model. It does **not** independently identify a human when the admin PIN is shared. The user has approved one authorized administrator both entering and confirming the same learner identity (D4-2); the resulting action must be labelled **self-reviewed**, never independently reviewed. A reviewer name typed by the operator remains a claim, not proof of individual identity. D4-3 is DELEGATED to role-only attribution: `actorRole=ADMIN`, `reviewMode=SELF_REVIEW`, with no operator-supplied human identity label. This still does not identify the individual human. School source/time policy remains unresolved.

A device-side review commit can use a durable transaction/revision/sequence receipt with read-back if the storage design proves it. No device signature is required by the selected remote-only threat model. Such a receipt proves only what the application/storage transaction can demonstrate; it is not tamper evidence against physical access. C13, C26 and C29 still block production persistence and recovery.

The approved network threat model does not decide which school records count as identity evidence, how review dates are trusted, how a changed identity invalidates prior reports, or whether reporting metadata may be added to the signed backup. Those remain OPEN.

## 5. Implementation gate

Do not expose real PII or authorize SF2 export until all applicable gates are resolved:

- C6: the HTTP adapter enforces the existing PIN/token/lockout contract;
- D4-4, D4-5 and D4-7: accepted sources, time semantics and revision/revocation policy; D4-3 is a delegated role-only attribution choice, not human identification;
- C13/C26/C29: durable commit, metadata storage and recovery/backup design;
- C28: bounded snapshot serving proven not to starve the serialized application loop;
- C31: the server, not browser-supplied flags, supplies source-bound reporting data and enforces the review gate.

D4-8 must not silently amend O8. The signed backup scope is config + roster + log. Adding reporting metadata to backups requires an explicit product-decision change; otherwise the report metadata remains unrecoverable after restore and authoritative export stays blocked.
