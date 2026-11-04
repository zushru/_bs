# SPEC: fileset + month_set (lib/core/src/fileset.h, month_set.h/.cpp)

Status: DELEGATED design. Host tests: test_monthset (10) plus the quarantine cases in test_hardening. Device: run once. The LittleFS driver is NOT written.

## IFileSet (implemented by the driver over LittleFS)
count(); monthKey(i) strictly increasing (e.g. 202611); fileSize(i); readFile(i, off, buf, n); appendFile(i, buf, n) durable or false; truncateFile(i, n); addFile(monthKey) new empty file at the end; quarantineCopy(i) copy file i aside and keep the original; quarantineFrom(i) move files i..end aside.

## MonthSet : IStorage
Presents month files as one append-only byte stream, oldest first. LogStore sits on top unchanged.

| Operation | Rule |
|---|---|
| setMonth(key) | Caller sets from the RTC before appending. Month 0 = unset |
| append | Refused if month unset. New file when key > newest key (refused if the newest file size is not a multiple of 16). Same or earlier key appends to the newest file |
| truncate(n) | Only inside the last non-empty file. Otherwise refused, and refusedTruncate() = n |
| planAt(off) | Returns {valid, fileIndex, localOff} for the record at a byte offset |
| repair(plan) | quarantineCopy(file), quarantineFrom(next files), truncateFile(file, localOff), clears the refusal. Safe to re-plan after a crash |

## Known gaps
- Driver and atomic behaviour of LittleFS rename/copy under power loss: UNVERIFIED.
- Quarantined months are kept on flash but nothing reads them back. No restore path.
- EV_REPAIR (defined, unused). Proposed: a = files quarantined. NOT approved.
- Monthly files store every record; per-month size against the flash partition is unmeasured (spiffs partition 0x270000 per handoff).
