#!/usr/bin/env python3
"""Static contract check for ATT's canonical read-only dashboard."""
from __future__ import annotations

import hashlib
import importlib.util
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML_PATH = ROOT / "web/dashboard/index.html"
HEADER_PATH = ROOT / "target/esp32_attendance/dashboard_html.h"
GENERATOR_PATH = ROOT / "tools/embed_dashboard.py"
MAIN_PATH = ROOT / "target/esp32_attendance/main.cpp"

errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def main() -> int:
    for path in (HTML_PATH, HEADER_PATH, GENERATOR_PATH, MAIN_PATH):
        require(path.is_file(), f"required dashboard integration file missing: {path.relative_to(ROOT)}")
    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1

    html = HTML_PATH.read_text(encoding="utf-8")
    header = HEADER_PATH.read_text(encoding="utf-8")
    firmware = MAIN_PATH.read_text(encoding="utf-8")

    spec = importlib.util.spec_from_file_location("att_dashboard_generator", GENERATOR_PATH)
    if spec is None or spec.loader is None:
        errors.append("cannot load dashboard header generator")
    else:
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        require(not module._RUN_AS_PLATFORMIO,
                "importing dashboard generator must not invoke PlatformIO or write files")
        require(header == module.render_header(html),
                "generated PROGMEM header does not exactly match canonical HTML source")

    expected_pages = (
        "dashboard", "attendance", "students", "timetable", "reports", "device"
    )
    found_pages = tuple(re.findall(r'data-page="([a-z]+)"', html))
    require(found_pages == expected_pages,
            f"dashboard navigation mismatch: expected {expected_pages!r}, found {found_pages!r}")
    require(html.lower().startswith("<!doctype html>") and "</html>" in html.lower(),
            "dashboard is not a complete HTML document")
    require('name="robots" content="noindex,nofollow"' in html,
            "local dashboard must discourage indexing")
    require(len(re.findall(r"<script(?:\s|>)", html, re.I)) == 1,
            "dashboard must contain exactly one inline script")
    require(not re.search(r"<script[^>]+\bsrc\s*=", html, re.I),
            "dashboard must not load external scripts")
    require(not re.search(r"<link[^>]+\bhref\s*=", html, re.I),
            "dashboard must not load external stylesheets or remote assets")
    require(not re.search(r"<form(?:\s|>)", html, re.I),
            "read-only dashboard must not contain mutation forms")

    require(html.count('fetch("/api/status"') == 1 and html.count("fetch(") == 1,
            "dashboard must issue exactly one same-origin GET /api/status request")
    for token in (
        'method:"GET"', 'cache:"no-store"', 'credentials:"same-origin"',
        "AbortController", "5000", "clearTimeout(timeout)",
        "window.setInterval(refreshStatus,15000)", "response.ok"
    ):
        require(token in html, f"dashboard request safety contract missing: {token}")
    for forbidden in (
        "sampleStudents", "DEMO_TODAY", "scenario-select", "localStorage",
        "sessionStorage", "XMLHttpRequest", "HTTP_POST", "attendance.csv",
        "simulate-", "wifipass", "passphrase", "password"
    ):
        require(forbidden.lower() not in html.lower(),
                f"dashboard contains forbidden demo, mutation, persistence or secret token: {forbidden}")
    require("unverified" in html.lower() and "sensor_fault" in html,
            "dashboard must preserve unknown coverage and sensor-fault states")
    require('$("page-content").innerHTML=PAGES[state.page]' in html and
            'node.textContent=displayValue(value,fallback)' in html and
            html.count("alert.innerHTML='<span") == 2,
            "page navigation must use fixed templates and API values must be rendered as text")
    status_view = re.search(
        r"function updateStatusView\(\)\s*\{(?P<body>.*?)\n\}", html, re.S
    )
    require(status_view is not None,
            "status polling must have a dedicated in-place view update")
    if status_view:
        require("renderHeader();" in status_view.group("body") and
                "applySnapshot();" in status_view.group("body"),
                "in-place status update must refresh header and visible status fields")
        require("innerHTML" not in status_view.group("body") and
                "closeSidebar" not in status_view.group("body"),
                "status updates must not rebuild page content or close navigation")
    header_renderer = re.search(
        r"function renderHeader\(\)\s*\{(?P<body>.*?)\n\}", html, re.S
    )
    require(header_renderer is not None,
            "status header renderer must remain present")
    if header_renderer:
        body = header_renderer.group("body")
        require('alert.dataset.mode!=="offline"' in body and
                'alert.dataset.mode="offline"' in body and
                'alert.dataset.mode!=="readonly"' in body and
                'alert.dataset.mode="readonly"' in body,
                "aria-live status notice must update only when connectivity mode changes")
    refresh = re.search(
        r"async function refreshStatus\(\)\s*\{(?P<body>.*?)\n\}", html, re.S
    )
    require(refresh is not None,
            "bounded status refresh function must remain present")
    if refresh:
        require("updateStatusView();" in refresh.group("body") and
                "renderPage(" not in refresh.group("body"),
                "status polling must update existing nodes without rerendering the page")

    require(firmware.count('app.http.on("') == 2 and
            'app.http.on("/", HTTP_GET' in firmware and
            'app.http.on("/api/status", HTTP_GET' in firmware,
            "firmware must expose only the two expected GET routes")
    require('#include "dashboard_html.h"' in firmware and
            'app.http.send_P(200, "text/html; charset=utf-8", kDashboardHtml);' in firmware,
            "firmware root route must serve the generated canonical page")
    require("kNeedsSetupHtml" not in firmware and "kConfiguredHtml" not in firmware,
            "legacy duplicate page strings must not remain in the composition root")
    require("script-src 'unsafe-inline'" in firmware and
            "connect-src 'self'" in firmware and
            "frame-ancestors 'none'" in firmware,
            "firmware CSP must permit only the embedded script and same-origin status fetch")
    require('HTTP_POST' not in firmware and 'HTTP_PUT' not in firmware and
            'HTTP_DELETE' not in firmware and 'HTTP_PATCH' not in firmware,
            "read-only boot shell must not expose mutation methods")

    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1

    digest = hashlib.sha256(html.encode("utf-8")).hexdigest()
    print("PASS: canonical HTML/header parity")
    print("PASS: six reference destinations")
    print("PASS: self-contained, non-indexed UI")
    print("PASS: status-only, no-store GET with bounded refresh")
    print("PASS: no demo state, credential fields or mutation controls")
    print("PASS: read-only firmware route surface and CSP")
    print("HTML SHA-256:", digest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
