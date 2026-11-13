# SPEC: diagnostic sketch (sketch/sketch_compact_minimal_fixed.ino)

Status: bring-up tool only. NOT production firmware. Built and run by you in ArduinoDroid. Last device run: template round-trip byte-exact at 128-byte packets.

## Pins (3V3 only, no 5 V on the AS608)
| Signal | GPIO | Note |
|---|---|---|
| Buzzer | 23 | LOW = on |
| I2C SDA / SCL | 21 / 22 | OLED 0x3C, DS3231 0x68, EEPROM 0x57 on the RTC board |
| AS608 TX -> ESP32 RX2 | 16 | UART2 at 57600 8N1 |
| ESP32 TX2 -> AS608 RX | 17 | |
| AS608 touch (TCH) | 4 | digital input |
| AS608 V+, VA | 3V3 | |
| AS608 D+, D- | open | |

## Access
Wi-Fi AP "ATT-DIAG", passphrase diag12345, page at http://192.168.4.1. Endpoints: / (page), /log, /run?j=N, /note?t=..., /dl (download diag.txt), /clr (clear log).

## Jobs
1 I2C scan; 2 RTC read; 3 GPIO4 sample 8 s; 4 AS608 params; 5 enroll slot 1; 6 10 scans (Wi-Fi on); 7 beep 50; 8 beep 100; 9 template round-trip; 10 10 scans with AP off; 11 read index table (read-only).

### Job 11: ReadIndexTable probe
- Authenticates with the existing sensor password, reads the reported capacity, then sends instruction `0x1F` for each index-table page needed by that capacity. At capacity 300 it reads table pages 0 and 1.
- Requires a response frame of exactly 44 bytes, ACK packet type `0x07`, packet length `0x0023`, confirmation code `0x00`, and a valid frame checksum. The 32 payload bytes are logged in four groups of eight.
- Prints two possible interpretations of occupied physical pages: LSB-first and MSB-first. Adafruit's CircuitPython `read_templates()` implementation uses LSB-first (byte bit 0 maps to the first slot in that byte). That is a useful library reference, not proof that this particular sensor/firmware matches it. Compare the output with known physical pages before closing U5.
- Job 11 is read-only. It does not enroll, delete, empty, or write any template.
- The generic module manual describes instruction `0x1F`, 32 bytes per table page, table page 0 as entries 0–255, and page 1 as 256–511. Adafruit CircuitPython's `read_templates()` independently uses LSB-first byte decoding. These are protocol/reference sources, not proof that every AS608 clone behaves identically.

### Verification procedure
1. Install the full archive and flash this diagnostic sketch from ArduinoDroid. Join AP `ATT-DIAG` (password `diag12345`) and open `http://192.168.4.1`.
2. Run Job 11 only. Download `/dl` or copy the log. Confirm that capacity is 300, both table pages return ACK `00`, and each ACK has a 32-byte payload. If either page times out, has a nonzero ACK, or has the wrong frame length, stop: U5 is not verified.
3. Compare both candidate page lists against **known physical page numbers** already stored in the sensor. Do not equate the diagnostic labels "slot 1/2" with product student IDs.
4. If Job 5 and Job 9 were both run on a sensor with no other templates in physical pages 0–7, their current sketch code writes physical pages 1 and 2. Therefore the first bitmap byte should be `06` for LSB-first, or `60` for MSB-first. This expectation is conditional; other templates in pages 0–7 change the byte. Do not re-run Job 5 or Job 9 merely to create test data, because they write to fixed pages without checking whether those pages are already occupied.
5. Send the complete Job 11 log when hardware is available. The production occupancy reader exists in v0.2.6, but it must remain disabled for production enrollment until there is a controlled known-page comparison and the page-numbering decision below.

## Known hazards (do not ignore)
- Job 9 stores the template into physical page 2 and never deletes it. The sensor then holds a second template for the same finger. No delete command exists in this sketch. (CRITICAL.md C5)
- Job 5 logs "slot 1" but writes physical page 1; Job 9 writes physical page 2. Core decision E18 maps student ID to `page = ID - 1` (ID 1 -> page 0) and remains DELEGATED. This discrepancy is intentionally not changed in the diagnostic probe. Do not use Job 5/9 labels as proof of the production mapping.
- LittleFS.begin(true) formats the flash if mounting fails. Acceptable for a diagnostic. Production must NEVER auto-format (handoff: explicit action only).
- The 10-scan pass line (>= 9 clean, no BROWNOUT) is a bring-up judgement, not a product rule.
- Code 23 and the final reply after DownChar are not interpreted. The verdict is the byte-exact re-download.
- Stub-compiled only (see TESTS.md). The real ESP32 core is the ground truth; it has built and run for you.
