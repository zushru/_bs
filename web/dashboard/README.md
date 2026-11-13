# ATT local dashboard

`index.html` is the canonical source for the ESP32 local dashboard. It is a self-contained document: no CDN, external font, remote script, external image, package install or browser build step is required.

## Integration

- Source: `web/dashboard/index.html`
- Generated firmware header: `target/esp32_attendance/dashboard_html.h`
- Generator: `tools/embed_dashboard.py`
- Firmware route: `GET /` in `target/esp32_attendance/main.cpp`
- Read-only data route: `GET /api/status`

Edit the HTML source, then run:

```sh
python3 tools/embed_dashboard.py
```

The PlatformIO attendance environment also runs the generator as a pre-build script. The generated header is committed so source review and non-PlatformIO tooling can inspect the exact embedded page. Do not edit the generated header directly.

## Current API contract

The only browser request is a same-origin, uncached `GET /api/status`. The endpoint is the source of truth. The dashboard does not write to the device, keep a browser-side roster, infer device time, cache a stale status as current, or synthesize attendance data.

The six reference destinations remain visible:

1. Dashboard — displays boot, AP, storage replay, event-group audit, roster-history, admission, RTC-observation and health fields returned by the device.
2. Attendance — unavailable until the production attendance owner, trusted clock, scheduler, durable write path and read API are integrated.
3. Students & fingerprints — unavailable until the production roster and AS608 owners and their authenticated read/write APIs are integrated.
4. Timetable — unavailable until validated configuration read/update APIs and historical schedule semantics are available.
5. Reports & exports — unavailable until authoritative reporting, roster metadata, coverage and export gates are closed.
6. Device & settings — read-only diagnostics. No credential or mutation controls are exposed.

Unavailable pages explain their prerequisites. They contain no fixture rows, sample names, simulated success actions or fake downloads. Do not add controls until a real endpoint, authorization boundary, persistence result and failure behavior exist.

## Security and display rules

- The current firmware exposes only public, allowlisted status fields. Never add secrets, AP credentials, PIN state, tokens, learner PII or raw configuration to this endpoint.
- Content Security Policy permits only same-origin status requests and the inline code/style embedded in this standalone page. Do not add external assets.
- Treat all API values as untrusted text. Render them with `textContent`; never interpolate them into HTML.
- A failed or malformed status request clears the current snapshot and displays an unavailable state. Browser time labels only the time of the browser's last request; it is not device time.
- Missing attendance is not Absent. No attendance or coverage values should appear until the device can prove them.
- Keep the six navigation destinations stable. Disabled/unavailable functions must explain why; preserve keyboard and mobile navigation.

This is a canonical read-only dashboard UI for the current boot shell, not a complete or production-ready attendance application.

## Static verification

From the repository root, run `python3 tools/check_dashboard.py`. It checks source/header parity, the six navigation destinations, status-only read requests, disabled demo/mutation surfaces, the two GET route boundary and CSP. It does not compile firmware or prove physical behavior. `python3 tools/embed_dashboard.py` regenerates the firmware header from the canonical HTML source.
