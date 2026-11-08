# BRING-UP (breadboard, 3V3 only)

## Pin map
See docs/SPECS/diag_sketch.md. Summary: buzzer 23, I2C 21/22, AS608 TX->16, RX<-17, touch 4, V+ and VA on 3V3, GND common.

## Sequence and pass lines (from the original plan)
0 Wire, flash, join ATT-DIAG, see BOOT line. 1 I2C scan: 0x3C, 0x57, 0x68 found. 2 RTC read and unplug test (OSF). 3 Reset and chirp check (accepted). 4 Beep 50 and 100 audible. 5 GPIO4 sample. 6 AS608 params: cap 300, security 3, packet code 2, baud 57600. 7 Enroll slot 1, then template round-trip byte-exact. 8 Ten scans with Wi-Fi on, ten with AP off: pass at >= 9 clean and no BROWNOUT. 9 Read-only index-table probe (Job 11; new in v0.2.5). 10 Soak on the bank: NOT DONE.

## Index table check (Job 11)
- Run Job 11 only to read the table; this job does not change sensor templates.
- At capacity 300, expect page 0 and page 1 to each return an ACK code `00`, packet length `0023`, and 32 bitmap bytes (44-byte full frame).
- Adafruit CircuitPython's reference implementation uses LSB-first. Job 11 still prints both interpretations for comparison. Compare the LSB-first list with the sensor's known physical pages; the exact sensor output is required before U5 can be closed.
- Critical mapping discrepancy: Job 5 writes physical page 1 while called "slot 1"; Job 9 writes physical page 2. Core E18 maps student ID 1 to page 0 and is DELEGATED. Do not use the sketch's slot labels to resolve production mapping.

## Results so far (your logs, 2026-11-03)
| Step | Result |
|---|---|
| 1 I2C | 0x3C, 0x57, 0x68 |
| 2 RTC | OSF=0. Time kept across USB power loss (15:11:59 to 15:15:29) |
| 4 Beeps | audible |
| 3 Chirp | present at boot, accepted as cosmetic |
| 6 Params | cap=300, security=3, pktcode=2, baud=57600 |
| 6 Touch GPIO4 | 18 transitions in 8 s |
| 7 Enroll slot 1 | success |
| 8 Ten scans, Wi-Fi on | 9 match, 1 code 23 |
| 8 Ten scans, AP off | 8 match, 1 timeout, 1 code 23 (timeout looks like a finger delay) |
| 7 Template round-trip | byte-exact at 128-byte packets, 768 bytes, 6 packets, after removing handleClient from the UART read path and adding setRxBufferSize(2048) |
| 9 Soak | NOT DONE |
| Brownout | none observed in the logs |

## Known bring-up hazards
- The round-trip leaves a second template on the sensor (C5). Empty the sensor before reuse.
- Code 23 is counted as a failure in the pass line but is not understood (U6).
- Pass line (>= 9 of 10) is a judgement you accepted, not a product rule.
