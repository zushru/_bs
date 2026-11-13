#include "esp32_first_run_http_handler.h"

#include <stdio.h>
#include <string.h>

namespace att {
namespace {

const char kSetupUri[] = "/api/v1/setup";

const char* authError(const PinAuthCode code, int& status) {
  switch (code) {
    case PAC_BAD_SECRET:
      status = 401;
      return "setup_code_invalid";
    case PAC_LOCKED:
      status = 429;
      return "setup_locked";
    case PAC_NOT_READY:
      status = 423;
      return "setup_auth_unavailable";
    case PAC_BAD_INPUT:
      status = 422;
      return "invalid_setup_code";
    case PAC_ACTION_REFUSED:
      status = 423;
      return "setup_refused";
    case PAC_STORAGE_ERROR:
    case PAC_VERIFY_ERROR:
    case PAC_ACTION_INDETERMINATE:
    default:
      status = 503;
      return "setup_auth_fault";
  }
}

}  // namespace

Esp32FirstRunHttpHandler::Esp32FirstRunHttpHandler(
    FirstRunSetupCodeBoundary& boundary, HealthFsm& health,
    MonotonicSecondsSource monotonicSeconds, bool& applicationFaulted)
    : boundary_(boundary), health_(health),
      monotonicSeconds_(monotonicSeconds),
      applicationFaulted_(applicationFaulted), body_(), rejected_(false),
      pendingStatus_(0), pendingError_(nullptr) {}

bool Esp32FirstRunHttpHandler::canHandle(
    WebServer& server, HTTPMethod method, const String& uri) {
  (void)server;
  return method == HTTP_POST && uri == kSetupUri;
}

bool Esp32FirstRunHttpHandler::canRaw(
    WebServer& server, const String& uri) {
  return server.method() == HTTP_POST && uri == kSetupUri;
}

bool Esp32FirstRunHttpHandler::canUpload(
    WebServer& server, const String& uri) {
  return server.method() == HTTP_POST && uri == kSetupUri;
}

void Esp32FirstRunHttpHandler::upload(
    WebServer& server, const String& uri, HTTPUpload& uploadData) {
  (void)server;
  if (uri != kSetupUri) return;
  if (uploadData.status == UPLOAD_FILE_START)
    reject_(415, "multipart_file_upload_not_supported");
}
int Esp32FirstRunHttpHandler::contentLength_(
    const String& value, size_t& out) const {
  if (value.length() == 0) return 411;
  size_t parsed = 0;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c < '0' || c > '9') return 400;
    const size_t digit = static_cast<size_t>(c - '0');
    if (digit > FIRST_RUN_SETUP_REQUEST_MAX_BYTES ||
        parsed > (FIRST_RUN_SETUP_REQUEST_MAX_BYTES - digit) / 10u)
      return 413;
    parsed = parsed * 10u + digit;
  }
  if (parsed == 0) return 400;
  out = parsed;
  return 0;
}

void Esp32FirstRunHttpHandler::wipeBody_() {
  body_.clear();
}

void Esp32FirstRunHttpHandler::sendReply_(
    WebServer& server, int status, const char* body) {
  server.sendHeader("Cache-Control", "no-store, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Referrer-Policy", "no-referrer");
  server.sendHeader("Content-Security-Policy",
                    "default-src 'none'; frame-ancestors 'none'");
  server.send(status, "application/json", body);
}

void Esp32FirstRunHttpHandler::reject_(int status, const char* error) {
  if (!rejected_) {
    pendingStatus_ = status;
    pendingError_ = error;
  }
  rejected_ = true;
  wipeBody_();
}

void Esp32FirstRunHttpHandler::raw(
    WebServer& server, const String& uri, HTTPRaw& rawData) {
  if (uri != kSetupUri) return;
  if (rawData.status != RAW_START && rejected_) return;

  if (rawData.status == RAW_START) {
    wipeBody_();
    rejected_ = false;
    pendingStatus_ = 0;
    pendingError_ = nullptr;

    const String transferEncoding = server.header("Transfer-Encoding");
    const String contentType = server.header("Content-Type");
    size_t declaredLength = 0;
    if (transferEncoding.length() != 0) {
      reject_(400, "transfer_encoding_not_supported");
      return;
    }
    if (!contentType.equalsIgnoreCase("application/json")) {
      reject_(415, "application_json_required");
      return;
    }
    const int lengthStatus =
        contentLength_(server.header("Content-Length"), declaredLength);
    if (lengthStatus != 0) {
      reject_(lengthStatus,
                      lengthStatus == 413 ? "request_body_too_large" :
                      lengthStatus == 411 ? "content_length_required" :
                                            "invalid_content_length");
      return;
    }
    if (server.clientContentLength() != static_cast<int>(declaredLength)) {
      reject_(400, "content_length_mismatch");
      return;
    }
    const FirstRunSetupBodyCode beginCode = body_.begin(declaredLength);
    if (beginCode != FRSB_OK) {
      reject_(beginCode == FRSB_TOO_LARGE ? 413 : 400,
                      beginCode == FRSB_TOO_LARGE
                          ? "request_body_too_large" : "invalid_content_length");
      return;
    }
    return;
  }

  if (rawData.status == RAW_WRITE) {
    const FirstRunSetupBodyCode appendCode = body_.append(
        rawData.buf, rawData.currentSize, rawData.totalSize);
    if (appendCode != FRSB_OK) {
      reject_(appendCode == FRSB_TOO_LARGE ? 413 : 400,
                      appendCode == FRSB_TOO_LARGE
                          ? "request_body_too_large" : "invalid_request_chunk");
      return;
    }
    return;
  }

  if (rawData.status == RAW_END) {
    if (body_.finish() != FRSB_OK) {
      reject_(400, "request_body_incomplete");
      return;
    }
    return;
  }

  if (rawData.status == RAW_ABORTED) {
    body_.abort();
    reject_(400, "request_body_incomplete");
  }
}

void Esp32FirstRunHttpHandler::dispatch_(WebServer& server) {
  if (monotonicSeconds_ == nullptr) {
    sendReply_(server, 503, "{\"error\":\"monotonic_clock_unavailable\"}");
    wipeBody_();
    return;
  }

  FirstRunSetupRequest request;
  memset(&request, 0, sizeof request);
  const FirstRunSetupRequestCode decoded =
      decodeFirstRunSetupRequest(body_.data(), body_.length(), request);
  wipeBody_();

  if (decoded != FRSR_OK) {
    clearFirstRunSetupRequest(request);
    sendReply_(server, 422, "{\"error\":\"invalid_setup_request\"}");
    return;
  }

  ConfigBootResult bootResult = {};
  FirstRunSetupDispatchResult result = boundary_.provision(
      health_.state(), request.setupCode, 6, monotonicSeconds_(),
      request.initialPin, 6, request.config, bootResult);
  clearFirstRunSetupRequest(request);

  if (result.code == FRSD_PROVISIONED) {
    sendReply_(server, 202,
        "{\"status\":\"provisioned\",\"reboot\":\"pending\"}");
    // Stop accepting new requests while OLED blanking/reboot is pending.
    applicationFaulted_ = true;
    return;
  }

  int status = 503;
  const char* error = "setup_unavailable";
  switch (result.code) {
    case FRSD_NOT_READY:
      status = 423; error = "setup_not_ready"; break;
    case FRSD_INVALID_CONFIG:
      status = 422; error = "invalid_config"; break;
    case FRSD_INVALID_PIN:
      status = 422; error = "invalid_initial_pin"; break;
    case FRSD_CREDENTIAL_UNAVAILABLE:
      status = 503; error = "ap_credential_unavailable"; break;
    case FRSD_AUTHENTICATION_REJECTED:
      error = authError(result.authentication.code, status);
      break;
    case FRSD_HEALTH_REJECTED:
      status = 423; error = "health_blocked"; break;
    case FRSD_PIN_PREPARATION_FAILED:
      status = 503; error = "initial_pin_preparation_failed"; break;
    case FRSD_PROVISION_FAILED:
      status = 503; error = "configuration_commit_failed"; break;
    case FRSD_PROVISIONED:
      break;
  }

  if (result.authentication.code == PAC_LOCKED &&
      result.authentication.retryAfterS != 0) {
    char retry[16] = {};
    (void)snprintf(retry, sizeof retry, "%lu",
                   static_cast<unsigned long>(
                       result.authentication.retryAfterS));
    server.sendHeader("Retry-After", retry);
  }
  char payload[96] = {};
  (void)snprintf(payload, sizeof payload, "{\"error\":\"%s\"}", error);
  sendReply_(server, status, payload);
}

bool Esp32FirstRunHttpHandler::handle(
    WebServer& server, HTTPMethod method, const String& uri) {
  if (method != HTTP_POST || uri != kSetupUri) return false;
  if (rejected_) {
    char payload[96] = {};
    const char* error = pendingError_ != nullptr
        ? pendingError_ : "request_rejected";
    const int status = pendingStatus_ != 0 ? pendingStatus_ : 400;
    (void)snprintf(payload, sizeof payload, "{\"error\":\"%s\"}", error);
    sendReply_(server, status, payload);
    wipeBody_();
    return true;
  }
  if (!body_.ready()) {
    sendReply_(server, 400, "{\"error\":\"request_body_unavailable\"}");
    wipeBody_();
    return true;
  }
  dispatch_(server);
  return true;
}

}  // namespace att
