# GitHub Codespaces development

Codespaces moves native tests and ESP32 compile/link work from the Android phone to a cloud VM. It does not replace the ESP32 board or provide a normal USB pass-through from the phone.

## Included environment

- Debian 12 dev container.
- PlatformIO Core **6.2.0**.
- Node.js **22** for the SF2 reference-model tests.
- C/C++ build tools.
- Existing PlatformIO files retain the ESP32 platform and library pins (Espressif32 8.0.0, Arduino-ESP32 3.3.12, and Adafruit libraries).

PlatformIO downloads the actual target packages into the Codespace home directory on first target build. The Codespace should use a 2-core machine initially.

## Commands

From the project root:

```sh
./tools/codespace-doctor.sh
./tools/codespace-test.sh
./tools/codespace-build-esp32.sh
```

The test command runs all native C++ tests, the SF2 Node suite, and JavaScript syntax checks. The build command compiles/links the inert adapter smoke image and diagnostic probe, then inspects the final probe ELF. Neither script flashes firmware.

## Open from Android

1. Sign in to GitHub in your browser.
2. Open `https://github.com/codespaces`.
3. Choose the repository branch containing the imported v0.2.48 tree and create a Codespace.
4. Choose the smallest available 2-core machine.
5. Wait for the first dev-container setup to finish, open the integrated terminal, and run the doctor/test/build commands above.
6. Use the browser-based VS Code editor. Commit and push changes regularly; the Codespace is not a replacement for version control.

## Cost control

GitHub Free personal accounts currently include 120 Codespaces core-hours and 15 GB-month of storage. A 2-core Codespace consumes two core-hours per active hour (at most 60 active hours if none of the included compute quota has already been used). A 4-core machine consumes quota twice as quickly as a 2-core machine. Check your own billing usage before starting. Only running Codespaces incur compute charges; stopped Codespaces still consume storage. Closing the browser tab does not stop a Codespace. Stop it explicitly when finished. GitHub also documents a 12-hour maximum Codespace lifetime.

References:
- https://docs.github.com/en/billing/concepts/product-billing/github-codespaces
- https://docs.github.com/en/codespaces/developing-in-a-codespace/creating-a-codespace-for-a-repository
- https://docs.github.com/en/codespaces/developing-in-a-codespace/stopping-and-starting-a-codespace

## Hardware boundary

Codespaces generally cannot access an ESP32 connected by USB to the Android phone. Do not run `pio run -t upload` in Codespaces. Flash locally when the phone/USB workflow is available. Device serial output remains the only evidence of physical behavior.
