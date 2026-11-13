#pragma once

#include <stdint.h>

#include "admin_session.h"
#include "clock_confirmation.h"

namespace att {

enum AdminClockDispatchCode : uint8_t {
  ACDC_AUTHORIZATION_DENIED = 0,
  ACDC_CONFIRMATION_INVOKED
};

struct AdminClockDispatchResult {
  AdminClockDispatchCode code;
  ApiResult authorization;
  // Meaningful only when code == ACDC_CONFIRMATION_INVOKED.
  ClockConfirmResult confirmation;
};

// Request-to-service seam for the protected EP_CLOCK operation.
//
// The caller supplies the exact token decoded from the current HTTP request;
// this class never accepts a cached tokenValid boolean. It authorizes against
// the HealthFsm already bound inside ClockConfirmationService, then invokes
// that same service only after A_OK.
//
// This class is synchronous and adds no mutex. Construct/use it on the one
// serialized application task that also owns AdminSessionBoundary, config,
// RTC, LogStore, MonthSet and every other log/storage writer. A multi-task
// server must put this entire dispatch and every competing writer behind one
// shared application lock; a clock-only lock is insufficient.
//
// The returned clockTrusted value describes only the verified service
// transaction; it does not mutate HealthFsm or grant attendance permissions.
// The application owner must update health separately after CCF_OK.
// This is not an HTTP route, credential verifier, durable request-ID mechanism
// or exactly-once retry layer. Never blindly retry an ambiguous clock result.
class AdminClockConfirmation {
 public:
  AdminClockConfirmation(AdminSessionBoundary& adminSessions,
                          ClockConfirmationService& clock)
      : adminSessions_(adminSessions), clock_(clock) {}

  AdminClockConfirmation(const AdminClockConfirmation&) = delete;
  AdminClockConfirmation& operator=(const AdminClockConfirmation&) = delete;

  // nowS is monotonic seconds from the same application clock used by token
  // expiry. requestToken must be the token from this request, or nullptr.
  AdminClockDispatchResult confirm(
      const uint8_t requestToken[16], uint32_t nowS, LogStore& log,
      MonthSet& months, const ClockConfirmRequest& request);

  bool usesOwners(const HealthFsm& health,
                  const ConfigBootCoordinator& configBoot) const {
    return clock_.usesOwners(health, configBoot);
  }

  bool clockBlocked() const { return clock_.blocked(); }

 private:
  AdminSessionBoundary& adminSessions_;
  ClockConfirmationService& clock_;
};

}  // namespace att
