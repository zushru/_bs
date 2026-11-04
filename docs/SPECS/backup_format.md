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

## Known gaps
- C2 (CRITICAL.md): no restore orchestrator, no atomic swap, no LogIndex replay, no closure replay of the staged log.
- KDF iteration count not chosen. Time to derive on the ESP32 unmeasured.
- Crypto adapter (mbedtls AES-GCM, PBKDF2) not written. Its call signatures are UNVERIFIED.
