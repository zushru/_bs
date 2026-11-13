# Partition-layout reconciliation — 2026-11-13

Status: recorded values conflict; complete live partition map and cause NOT established. This is an evidence note, not a product decision or permission to change flash geometry.

## Source authority agrees

The signed handoff, attendance partition CSV, attendance PlatformIO configuration and runtime guard agree:

| Entry | Start | Size | End (exclusive) |
|---|---:|---:|---:|
| nvs | 0x9000 | 0x5000 | 0xE000 |
| otadata | 0xE000 | 0x2000 | 0x10000 |
| factory | 0x10000 | 0x180000 | 0x190000 |
| spiffs | 0x190000 | 0x270000 | 0x400000 |

The signed layout ends at 0x400000. Attendance build configuration selects target/esp32_attendance/partitions_att.csv and declares 4 MB. The runtime guard requires spiffs at 0x190000 with size 0x270000. The signed handoff was not edited.

## Conflicting recorded device pair

Repository records a user-provided 2026-11-12 diagnostic pair:
- spiffs address: 0x290000
- spiffs size: 0x160000
- end (exclusive): 0x3F0000

Relative to signed geometry, this starts 0x100000 later, is 0x110000 smaller and ends 0x10000 earlier. If the physical flash is 4 MiB, that end leaves 0x10000 after the reported partition. The pair alone does not establish the physical flash size or any other partition entry.

The original diagnostic capture and a complete raw partition-table dump are not in the repository. Repeated documentation is not independent evidence. This pair cannot identify the factory-app size, adjacent data partitions, all gaps, physical flash capacity, or the firmware/tool that produced it. Cause remains unknown.

## Runtime interpretation

The hardware probe uses esp_partition_find_first() to locate label spiffs. ESP-IDF documents that its partition API works with entries in the partition table and that this convenience function returns the first matching entry. If the reported pair came from that API, it reflects the table registered by the device, not the CSV in the source tree.

ESP-IDF documents the default partition-table offset as 0x8000 and a 0x1000-byte table sector containing entries and an appended MD5 checksum. Its gen_esp32part.py utility converts a binary table back to CSV.

References:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/partition.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html
- https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html

## Safe evidence required

Acquire from the actual device without writing to flash:
1. The complete 0x1000-byte partition-table sector at offset 0x8000.
2. Flash chip identification/capacity in the same session.
3. Device identity, date, tool version and current running image identity.
4. SHA-256 of the original binary, preserved unchanged.
5. An offline CSV conversion using the matching ESP-IDF gen_esp32part.py utility.
6. A full comparison of every entry's label, type, subtype, offset, size and flags, including overlap, gap and capacity checks.

For esptool 5.x, the read-only commands are:

    esptool --chip esp32 --port PORT flash-id
    esptool --chip esp32 --port PORT read-flash 0x8000 0x1000 device-partition-table.bin
    sha256sum device-partition-table.bin

Do not substitute write-flash, erase-region, erase-flash, a bundled firmware upload, or a filesystem operation. Convert a copy of the saved binary offline.

## Probe flashing hazard

platformio.attendance.ini explicitly selects the signed attendance CSV. platformio.esp32-probe.ini does not set board_build.partitions. Earlier probe build logs show partitions.bin generation, but this repository does not establish that artifact as the signed attendance table. Do not flash the probe partitions.bin or a bundle containing it. Do not use the probe as a partition migration.

The probe's VFS routine performs temporary filesystem writes if its exact signed-layout guard passes. A matching map alone is not authorization to run that exercise.

## Disposition

- Signed source geometry: consistent.
- Recorded device SPIFFS pair: conflicting.
- Complete live device map: unavailable.
- Physical flash size: not verified.
- Cause: unknown.
- Signed table, runtime constants and attendance target configuration: unchanged.
- Flash, erase, mount, format, repair, quarantine, truncate and hardware action: not performed.
- C13.3 and partition-layout release gate: OPEN.

No source-only inference can authorize a geometry change.
