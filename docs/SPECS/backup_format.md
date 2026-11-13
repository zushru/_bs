# SPEC: backup_format (lib/core/src/backup_format.h/.cpp)

Status: DELEGATED design. Host tests: test_backup (9). Device: run once BEFORE template removal, not re-run since. Crypto is a toy in the tests, so the real cipher is UNVERIFIED.

## Scope (decided)
Backup = config + roster (inside the log records) + the full log. Fingerprint templates are NOT backed up (user decision: "cut template back-up"). Re-enrolling is the recovery path.

## Outer container
Header 37 bytes: "ATTB", version 1, PBKDF2 iterations u32 (must be in BK_ITER_MIN 1000 .. BK_ITER_MAX 5,000,000; the exact count is NOT chosen), salt 16, nonce 12. Then chunks: fin u8 (0/1), length u16 (<= 4096), ciphertext, tag 16.
- Each chunk is AEAD-sealed (AES-256-GCM expected, adapter-provided through ICipher). AAD = header || chunk index u32 || fin. Nonce = base nonce XOR chunk index.
- Reordering, truncation, header edits, trailing bytes and a wrong passphrase all fail authentication. Nothing unauthenticated is emitted.
- BackupWriter.finish() must be called. crc() and length() are over every outer byte written.

## Acknowledgement and rollover
- ackMatches(writer, crc, len) requires finished() and matching crc and length.
- rolloverAllowed(ackedSeq, lastSeq, typedConfirm, setupCodeOk) requires all of: ackedSeq == lastSeq > 0, typed confirmation, setup code OK.

## Inner plaintext (version 2)
"ATTI", version 2 (BK_INNER_VER), fsVersion u16, flags u8 = 0, config slot length u16 = 192, config slot (192 bytes, from config_store), record count u32, records (16 bytes each), crc32 trailer over everything before it.
The version-1 layout (template section) is rejected.

## RestoreValidator (streams into a staging storage)
Checks in order: magic, version, fsVersion == expected, config slot decodes, record count, every record decodes with seq == previous + 1, no unfinished commit group at the end, crc32 trailer, no trailing bytes. Optional observers (up to 4, added before any byte is fed) receive each validated record. Use one RosterStore observer and check malformed() == 0 and overflow() == false before accepting.

## Semantic staged replay audit (v0.2.14)
`RestoreReplayAudit` is a separate, one-shot step after `RestoreValidator.ok()`:
- It must be constructed only after `RestoreValidator.ok()`; it snapshots the validated config at construction. It requires the exact same `IStorage` object held by the validator and exact `records * REC_SIZE` stage length.
- It computes CRC32 over the staged record bytes and compares it with the CRC32 accumulated over the bytes accepted by the validator. This detects accidental mutation between validation and replay, including a changed record whose per-record CRC was recomputed. It is not a cryptographic MAC and does not protect against concurrent mutation during replay.
- It opens a fresh `LogStore` on staging and attaches a fresh `RosterStore`, the `ClosureEngine`-owned `LogIndex`, and `RestoreGroupAudit` before open/replay.
- It rejects storage read failures, record CRC/sequence faults, any torn/dropped/corrupt tail, count/last-sequence mismatch, unfinished or malformed event groups, malformed/overflowed roster state, and gap/close index overflow.
- Current commit-group shapes checked are: ROSTER SET header plus four payloads; ROSTER activate/deactivate single records; CLOSE plus zero to five bitmap payloads with exact bitmap population count and no unused high ID bits; GAP chunk groups of one cause with positive durations, contiguous chunks and no 32-bit end overflow, optionally followed by a valid CLOCK_SET pair; and standalone two-record CLOCK_SET pairs. Other current single-record event types must not carry MORE. Payloads cannot appear independently.
- The audit never calls `ClosureEngine::step()`: that method may append missing CLOSE records and would mutate the restored history. It rebuilds only the existing `LogIndex`; it does not synthesize closures or validate all attendance/business rules.
- v0.2.77 makes `LogStore::open()` inspect-only: a bad stage tail is preserved and `open()` fails before replaying any valid-looking prefix. `RestoreReplayAudit` then rejects the failed stage. Do not retry that stage in place and do not automatically delete it; retain the original staged bytes for controlled evidence handling.
- Allocate the audit statically or on the heap because `RosterStore` and `ClosureEngine` include fixed-size tables. The caller must exclusively own/freeze staging from validation through audit and any future swap. No stage lock is implemented here.

## Known gaps
- C2 (CRITICAL.md): no restore transaction coordinator, recovery journal, atomic storage swap, post-swap reopen, reboot recovery or acknowledgement protocol. `RestoreReplayAudit` is a pre-swap check only.
- KDF iteration count not chosen. Time to derive on the ESP32 unmeasured.
- Crypto adapter (mbedtls AES-GCM, PBKDF2) not written. Its call signatures are UNVERIFIED.
