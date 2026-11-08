# Specialized D4 audit — v0.2.28

Date: 2026-11-07
Scope: review of the user-approved D4-2 workflow decision and current D4 documentation.

## Decision recorded

The user approved that one authorized administrator may both enter and confirm the same learner identity. This is a **self-review** workflow. It must not be described as independent verification, dual control, or approval by a separate person. Recorded as R12 SIGNED in `docs/DECISIONS.md` and D4-2 SIGNED in `docs/sf2/D4_DECISIONS.md`.

## Critical distinctions retained

1. The shared admin PIN/session authorizes the `ADMIN` role; it does not prove which human operated the phone.
2. Self-review permission does not resolve attribution (D4-3). A typed name/label, if later approved, would be self-attested, not independently authenticated.
3. Self-review permission does not establish what school records are acceptable (D4-4), trusted review time (D4-5), or correction/revocation semantics (D4-7).
4. D4-8 remains open. O8 is unchanged: backup = config + roster + log.
5. No cryptographic signing, secure element or physical-flash protection is required or claimed under R11.

## Implementation boundary

No verifier or review transition was implemented. No C++ or JavaScript source, API, persistence, PII exposure, browser UI or test registration changed. D3 remains `INCOMPLETE`; calculations remain `PREVIEW_READY` with `authoritativeExportReady:false`. No authoritative SF2 export is enabled.

## Verification accounting

- Documentation-only change. No functional tests, static source checks, C++ compilation, PlatformIO build, ESP32 build or device test was run.
- C++ registrations remain 376 across 23 suites; 318/318 is user-reported only for v0.2.17 and the later 58 registrations remain unrun.
- SF2 Node registrations remain 256; all remain unrun.
- The archive manifest and extracted archive are verified separately at release time.

## Open decisions and blockers

- D4-3: role-only attribution or self-attested operator label.
- D4-4: school-approved source categories and minimal reference policy.
- D4-5: review time and ordering policy.
- D4-7: revision invalidation and historical report behavior.
- D4-8: metadata backup/recovery policy; do not silently amend O8.
- C6, C13, C26, C27, C28, C29 and C31 remain production gates.

## Verdict

The D4-2 decision is recorded consistently. This release is a policy/documentation checkpoint only. It does not reduce the technical blockers for real PII persistence or authoritative export.
