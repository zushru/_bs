#pragma once

#include <stdint.h>

#include "clock_policy.h"
#include "config_boot.h"
#include "ds3231_clock.h"
#include "health_fsm.h"
#include "littlefs_mount_owner.h"
#include "month_set.h"

namespace att {

class IClockMonotonicSource {
 public:
  virtual ~IClockMonotonicSource() {}
  // Must return elapsed real microseconds (not wall-clock time) from a
  // non-decreasing source that remains valid for the complete service call.
  // Failure or counter regression makes clock confirmation fail closed.
  virtual bool nowMicros(uint64_t& out) = 0;
};

enum ClockConfirmCode : uint8_t {
  CCF_OK = 0,
  CCF_SERVICE_LATCHED,
  CCF_BAD_INPUT,
  CCF_LOG_NOT_READY,
  CCF_RTC_READ_FAILED,
  CCF_CONFIRMATION_REQUIRED,
  CCF_BLOCKED_WINDOW,
  CCF_POLICY_REJECTED,
  CCF_RTC_WRITE_NOT_STARTED,
  CCF_RTC_WRITE_REJECTED,
  CCF_RTC_WRITE_UNCERTAIN,
  CCF_RTC_VERIFY_FAILED,
  CCF_OSF_CLEAR_FAILED,
  CCF_LOG_APPEND_REJECTED,
  CCF_LOG_WAS_FAULTED,
  CCF_LOG_REPLAY_FAILED,
  CCF_LOG_COMMIT_INCOMPLETE,
  CCF_MONOTONIC_FAILED,
  CCF_HEALTH_BLOCKED,
  CCF_STORAGE_MISMATCH,
  CCF_CONFIG_NOT_READY,
  CCF_STORAGE_OWNER_NOT_READY,
  CCF_STORAGE_OWNER_MISMATCH
};

struct ClockConfirmRequest {
  uint32_t requestedUtcEpoch;
  bool confirmBackward;
  bool confirmJump;
};

struct ClockConfirmResult {
  ClockConfirmCode code;
  ClockAction action;
  ClockApplyResult applyResult;
  uint32_t previousUtcEpoch;
  uint32_t confirmedUtcEpoch;
  uint32_t replayBaselineEpoch;
  // True only means the log/RTC transaction was fully verified. It does not
  // mutate HealthFsm or grant attendance permission; the serialized caller
  // must separately update health while preserving independent fault domains.
  bool clockTrusted;
};

// Executes one explicit clock confirmation. This is NOT an authentication
// boundary: a future HTTP/application caller must invoke it only after
// request-bound administrator authentication and explicit confirmation.
// The service owns no HTTP, PIN, token, or session state.
class ClockConfirmationService {
 public:
  // These authoritative owners must outlive this service. Config/health/
  // storage state updates and service calls share one serialized app owner.
  ClockConfirmationService(Ds3231Clock& rtc, IClockMonotonicSource& monotonic,
                           const HealthFsm& health,
                           const ConfigBootCoordinator& configBoot,
                           ILittleFsStorageOwner& storageOwner)
      : rtc_(rtc), monotonic_(monotonic), health_(health),
        configBoot_(configBoot), storageOwner_(storageOwner), blocked_(false) {}

  // Preconditions:
  // - the bound ConfigBootCoordinator must currently be CFG_BOOT_READY.
  // - the bound HealthFsm must be H_OK, H_CLOCK_FAULT or H_SENSOR_FAULT.
  // - the bound storage owner must be LITTLEFS_OWNER_READY and expose its
  //   serialized IFileSet; months must reference that exact object.
  // - request values come from a request-bound authenticated admin action.
  // - log must be backed by the exact MonthSet supplied to confirm().
  // - the service derives the destination YYYYMM from requested UTC plus
  //   the current validated config offset before appending; older keys still
  //   append to the newest file under MonthSet's append-only ordering rule.
  // - caller serializes this complete call with all application writers.
  // The service appends/replays/verifies the CLOCK_SET intent before RTC writes.
  // It then writes/read-backs UTC and explicitly clears/verifies OSF.
  // No durable request ID exists: never blindly retry an ambiguous result.
  // Any unresolved post-append outcome permanently blocks this instance.
  // A CCF_OK result does not mutate HealthFsm: the serialized application
  // owner must separately feed the verified clock acceptance into health
  // while retaining current sensor/storage/config health inputs. Never treat
  // result.clockTrusted alone as attendance authorization.
  ClockConfirmResult confirm(LogStore& log, MonthSet& months,
                             const ClockConfirmRequest& request);

  bool blocked() const { return blocked_; }

  // Identity check for the serialized application owner.
  bool usesOwners(const HealthFsm& health,
                  const ConfigBootCoordinator& configBoot) const {
    return &health_ == &health && &configBoot_ == &configBoot;
  }

  // Read-only state from the exact HealthFsm used by confirm(). The
  // request-bound EP_CLOCK dispatcher uses this instead of accepting a
  // caller-selected health value that could differ from the service's owner.
  HState healthState() const { return health_.state(); }

 private:
  Ds3231Clock& rtc_;
  IClockMonotonicSource& monotonic_;
  const HealthFsm& health_;
  const ConfigBootCoordinator& configBoot_;
  ILittleFsStorageOwner& storageOwner_;
  bool blocked_;
};

}  // namespace att
