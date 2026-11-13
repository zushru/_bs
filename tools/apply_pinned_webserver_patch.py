# Hash-pinned, temporary patch for the attendance target's bundled WebServer.
Import("env")

import atexit
import hashlib
import json
import os
import stat
import tempfile
from pathlib import Path

EXPECTED_ENV = "att-boot-shell"
PACKAGE_NAME = "framework-arduinoespressif32"
PACKAGE_VERSION = "4.30312.261024"
WEBSERVER_VERSION = "3.3.12"
PARSING_BEFORE_SHA256 = "c24f1e65b4ce3fb7ab868f6bca7aab7af5dcdfd1ca81ce9716752a922e52ce78"
PARSING_AFTER_SHA256 = "bead61f5bddfc37c66a6b85bde0860aec1d1382c57b1093e68a77d7acb229703"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fail(message):
    raise RuntimeError("ATT WebServer patch refused: " + message)


def atomic_write(path, data, mode):
    fd, temporary = tempfile.mkstemp(prefix="." + path.name + ".att-", dir=str(path.parent))
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.chmod(temporary, mode)
        os.replace(temporary, str(path))
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def property_version(path):
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values.get("version")


if env.subst("$PIOENV") != EXPECTED_ENV:
    fail("script is permitted only for the att-boot-shell environment")

package_dir_value = env.PioPlatform().get_package_dir(PACKAGE_NAME)
if not package_dir_value:
    fail("pinned Arduino framework package is not installed")
package_dir = Path(package_dir_value)

try:
    package_metadata = json.loads((package_dir / "package.json").read_text(encoding="utf-8"))
except (OSError, ValueError) as error:
    fail("cannot read framework package metadata: " + str(error))

if package_metadata.get("name") != PACKAGE_NAME:
    fail("framework package name does not match the expected package")
if package_metadata.get("version") != PACKAGE_VERSION:
    fail("framework package version drifted")
if property_version(package_dir / "libraries" / "WebServer" / "library.properties") != WEBSERVER_VERSION:
    fail("bundled WebServer library version drifted")

parsing_path = package_dir / "libraries" / "WebServer" / "src" / "Parsing.cpp"
backup_path = parsing_path.with_name("Parsing.cpp.att-original")
source_mode = stat.S_IMODE(parsing_path.stat().st_mode)

# Recover a source left patched by an abruptly terminated previous build.
if backup_path.exists():
    backup_bytes = backup_path.read_bytes()
    if sha256(backup_bytes) != PARSING_BEFORE_SHA256:
        fail("retained framework backup does not match the pinned original")
    current_hash = sha256(parsing_path.read_bytes())
    if current_hash == PARSING_AFTER_SHA256:
        atomic_write(parsing_path, backup_bytes, source_mode)
    elif current_hash != PARSING_BEFORE_SHA256:
        fail("framework source and retained backup are both unexpected")
    backup_path.unlink()

source = parsing_path.read_bytes()
if sha256(source) == PARSING_AFTER_SHA256:
    fail("framework is patched without its recovery backup")
if sha256(source) != PARSING_BEFORE_SHA256:
    fail("Parsing.cpp source hash drifted; review the new upstream version")

replacements = (
    (
        b'static const char filename[] PROGMEM = "filename";\n',
        b'static const char filename[] PROGMEM = "filename";\n\n'
        b'// Raw callbacks must bound request bodies before any form parser can allocate.\n'
        b'static const size_t kWebServerMaxRawBodyLen = 768;\n',
    ),
    (
        b'    bool isForm = false;\n    bool isEncoded = false;\n',
        b'    bool isForm = false;\n    bool isEncoded = false;\n'
        b'    size_t contentLengthHeaderCount = 0;\n'
        b'    size_t contentTypeHeaderCount = 0;\n'
        b'    size_t transferEncodingHeaderCount = 0;\n',
    ),
    (
        b'      if (headerName.equalsIgnoreCase(FPSTR(Content_Type))) {',
        b'      if (headerName.equalsIgnoreCase(FPSTR(Content_Type))) {\n        ++contentTypeHeaderCount;',
    ),
    (
        b'''      } else if (headerName.equalsIgnoreCase(F("Content-Length"))) {
        _clientContentLength = headerValue.toInt();
      } else if (headerName.equalsIgnoreCase(F("Host"))) {''',
        b'''      } else if (headerName.equalsIgnoreCase(F("Content-Length"))) {
        ++contentLengthHeaderCount;
        _clientContentLength = headerValue.toInt();
      } else if (headerName.equalsIgnoreCase(F("Transfer-Encoding"))) {
        ++transferEncodingHeaderCount;
      } else if (headerName.equalsIgnoreCase(F("Host"))) {''',
    ),
    (
        b'    if (!isForm && _currentHandler && _currentHandler->canRaw(*this, _currentUri)) {',
        b'''    if (_currentHandler == nullptr ||
        !_currentHandler->canRaw(*this, _currentUri)) {
      sendErrorResponse(client, "405 Method Not Allowed");
      return false;
    }
    if (contentLengthHeaderCount > 1 || contentTypeHeaderCount > 1 ||
        transferEncodingHeaderCount > 0) {
      sendErrorResponse(client, "400 Bad Request");
      return false;
    }

    if (_currentHandler && _currentHandler->canRaw(*this, _currentUri)) {''',
    ),
    (
        b'''      while (_currentRaw->totalSize < (size_t)_clientContentLength) {
        size_t read_len = std::min((size_t)_clientContentLength - _currentRaw->totalSize, (size_t)HTTP_RAW_BUFLEN);
        _currentRaw->currentSize = client.readBytes(_currentRaw->buf, read_len);
        _currentRaw->totalSize += _currentRaw->currentSize;
        if (_currentRaw->currentSize == 0) {
          _currentRaw->status = RAW_ABORTED;
          _currentHandler->raw(*this, _currentUri, *_currentRaw);
          return false;
        }
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
      }''',
        b'''      const unsigned long rawBodyStart = millis();
      if (_clientContentLength < 0) {
        _currentRaw->status = RAW_ABORTED;
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
        sendErrorResponse(client, "400 Bad Request");
        return false;
      }
      if (static_cast<size_t>(_clientContentLength) >
          kWebServerMaxRawBodyLen) {
        _currentRaw->status = RAW_ABORTED;
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
        sendErrorResponse(client, "413 Content Too Large");
        return false;
      }
      const size_t requestBodyLength =
          static_cast<size_t>(_clientContentLength);
      while (_currentRaw->totalSize < requestBodyLength) {
        if (static_cast<unsigned long>(millis() - rawBodyStart) >=
            HTTP_MAX_POST_WAIT) {
          _currentRaw->status = RAW_ABORTED;
          _currentHandler->raw(*this, _currentUri, *_currentRaw);
          if (client.connected())
            sendErrorResponse(client, "408 Request Timeout");
          return false;
        }
        const int available = client.available();
        if (available <= 0) {
          if (!client.connected()) {
            _currentRaw->status = RAW_ABORTED;
            _currentHandler->raw(*this, _currentUri, *_currentRaw);
            return false;
          }
          delay(1);
          continue;
        }
        const size_t remaining =
            requestBodyLength - _currentRaw->totalSize;
        const size_t read_len = std::min(
            std::min(static_cast<size_t>(available), remaining),
            static_cast<size_t>(HTTP_RAW_BUFLEN));
        const int bytesRead = client.read(_currentRaw->buf, read_len);
        _currentRaw->currentSize = bytesRead > 0
            ? static_cast<size_t>(bytesRead)
            : 0;
        _currentRaw->totalSize += _currentRaw->currentSize;
        if (_currentRaw->currentSize == 0) {
          _currentRaw->status = RAW_ABORTED;
          _currentHandler->raw(*this, _currentUri, *_currentRaw);
          if (client.connected())
            sendErrorResponse(client, "408 Request Timeout");
          return false;
        }
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
      }''',
    ),
)

patched = source
for old, new in replacements:
    if patched.count(old) != 1:
        fail("a pinned parser patch anchor is missing or duplicated")
    patched = patched.replace(old, new)

if sha256(patched) != PARSING_AFTER_SHA256:
    fail("prepared parser patch does not match the reviewed after-image")

atomic_write(backup_path, source, 0o600)


def restore_framework_source():
    if not backup_path.exists():
        return
    try:
        current_hash = sha256(parsing_path.read_bytes())
        if current_hash == PARSING_AFTER_SHA256:
            atomic_write(parsing_path, source, source_mode)
        elif current_hash != PARSING_BEFORE_SHA256:
            print("ATT WebServer patch warning: source changed during build; retaining the original backup")
            return
        backup_path.unlink()
        print("ATT WebServer patch: original framework source restored")
    except OSError as error:
        print("ATT WebServer patch warning: restore failed: " + str(error))


atexit.register(restore_framework_source)
atomic_write(parsing_path, patched, source_mode)
if sha256(parsing_path.read_bytes()) != PARSING_AFTER_SHA256:
    fail("patched framework source failed its SHA-256 read-back")

print("ATT WebServer patch: bounded raw-only body parser enabled for this build (768 bytes / 5000 ms)")
