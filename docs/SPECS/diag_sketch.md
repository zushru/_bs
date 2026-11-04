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
1 I2C scan; 2 RTC read; 3 GPIO4 sample 8 s; 4 AS608 params; 5 enroll slot 1; 6 10 scans (Wi-Fi on); 7 beep 50; 8 beep 100; 9 template round-trip; 10 10 scans with AP off.

## Known hazards (do not ignore)
- Job 9 stores the template into page 2 and never deletes it. The sensor then holds a second template for the same finger. Under idToPage, page 2 is ID 2. Empty the sensor before any production use. No delete command exists in this sketch. (CRITICAL.md C5)
- LittleFS.begin(true) formats the flash if mounting fails. Acceptable for a diagnostic. Production must NEVER auto-format (handoff: explicit action only).
- The 10-scan pass line (>= 9 clean, no BROWNOUT) is a bring-up judgement, not a product rule.
- Code 23 and the final reply after DownChar are not interpreted. The verdict is the byte-exact re-download.
- Stub-compiled only (see TESTS.md). The real ESP32 core is the ground truth; it has built and run for you.
