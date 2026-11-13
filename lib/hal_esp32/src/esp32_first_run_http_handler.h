#pragma once

#include <stddef.h>
#include <stdint.h>

#include <WebServer.h>

#include "first_run_setup_request.h"
#include "health_fsm.h"
#include "setup_code_sequence.h"

namespace att {

class Esp32FirstRunHttpHandler : public RequestHandler {
 public:
  typedef uint32_t (*MonotonicSecondsSource)();

  Esp32FirstRunHttpHandler(FirstRunSetupCodeBoundary& boundary,
                           HealthFsm& health,
                           MonotonicSecondsSource monotonicSeconds,
                           bool& applicationFaulted);

  bool canHandle(WebServer& server, HTTPMethod method,
                 const String& uri) override;
  bool canRaw(WebServer& server, const String& uri) override;
  bool canUpload(WebServer& server, const String& uri) override;
  bool handle(WebServer& server, HTTPMethod method,
              const String& uri) override;
  void raw(WebServer& server, const String& uri, HTTPRaw& raw) override;
  void upload(WebServer& server, const String& uri,
              HTTPUpload& upload) override;

 private:
  void wipeBody_();
  void reject_(int status, const char* error);
  void sendReply_(WebServer& server, int status, const char* body);
  void dispatch_(WebServer& server);
  int contentLength_(const String& value, size_t& out) const;

  FirstRunSetupCodeBoundary& boundary_;
  HealthFsm& health_;
  MonotonicSecondsSource monotonicSeconds_;
  bool& applicationFaulted_;
  FirstRunSetupBodyBuffer body_;
  bool rejected_;
  int pendingStatus_;
  const char* pendingError_;
};

}  // namespace att
