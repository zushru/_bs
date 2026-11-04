# UNVERIFIED (claims that are not yet proven, and the test that would settle each)

| # | Claim | Status | Settle it by |
|---|---|---|---|
| U1 | 149 host tests pass on the device with the real Unity | Unverified | `pio test -e native` on the device |
| U2 | Real Unity agrees with the stand-in assertions | Unverified (the 142 device run used the same files, so the stand-in matches on those) | Same as U1 |
| U3 | AS608 template is 768 bytes | Measured on one sensor twice | Enroll a second sensor |
| U4 | 128-byte packets work for template transfer | Measured on one sensor | Same |
| U5 | ReadIndexTable exists, page size 32 bytes, bit order LSB first | Unverified | Read the index on the device, compare with enrollments |
| U6 | Code 23 is an image-quality error | Unverified | Datasheet, or log the error and vary finger placement |
| U7 | ID 300 maps to page 299 on a 300-capacity sensor | Unverified | Enroll ID 300 and scan it |
| U8 | Template round-trip leaves a duplicate (page 2) | Confirmed by reading the code | Sensor state after job 9 |
| U9 | GPIO23 buzzer does not chirp at boot like GPIO25 | Partly: chirp present, you accepted it | Accepted by you as cosmetic. No action |
| U10 | 3V3 holds through a Wi-Fi scan burst without a bulk capacitor | Inferred from 9 of 10 clean scans, no BROWNOUT | Measure the rail or repeat with a capacitor |
| U11 | setRxBufferSize(2048) works on your ESP32 core | Verified: the sketch runs | None |
| U12 | Bank lasts 10 hours with the AP on | Unverified | 10-hour soak |
| U13 | RTC keeps time through USB power loss | Verified once (15:11:59 to 15:15:29, OSF=0) | Repeat |
| U14 | LittleFS rename and truncate are atomic on power loss | Unverified | Power-cut test |
| U15 | mbedtls AES-GCM and PBKDF2 are available and fast enough | Unverified | Build and time on the device |
| U16 | Duplicate detection threshold | Unverified, no data | Two-person enroll test with logged scores |
| U17 | Boot rebuild time of the index at full log size | Unmeasured | Time it on the device |
| U18 | Restore timing | Unmeasured | Time it on the device |
| U19 | Handoff future-day rule | Missing from the handoff (C9) | Your decision |
| U20 | GET /status exposure is acceptable | Not approved | Your decision |
| U21 | Stub-compiled sketch matches the real ESP32 API | Unverified beyond the device build | Device build |
