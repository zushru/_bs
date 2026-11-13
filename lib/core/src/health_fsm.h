#pragma once
#include <stdint.h>
namespace att {
enum HState : uint8_t { H_OK = 0, H_UNPROVISIONED, H_CLOCK_FAULT, H_STORAGE_FAULT, H_CONFIG_FAULT, H_SENSOR_FAULT };
const uint8_t HB_STORAGE = 1, HB_CONFIG = 2, HB_UNPROV = 4, HB_CLOCK = 8, HB_SENSOR = 16;
struct HealthInputs {
  bool provisioned, rtcPresent, rtcLostPower, epochBelowFloor, epochBelowLog;
  bool storageOk, cfgOk, sensorOk;
  bool reverified;   // one-shot: a full verification pass succeeded since the fault
  bool clockTrusted; // explicit acceptance after RTC/log reconciliation; plausibility alone is insufficient
};
struct Perms { bool scan, manual, enroll, readHistory, exportOk; };
class HealthFsm {
 public:
  HealthFsm() : latch_(0), mask_(0), state_(H_UNPROVISIONED) {}
  // clockReverified releases only HB_CLOCK; reverified retains its legacy all-domain meaning.
  HState update(const HealthInputs& in, bool clockReverified = false);
  HState state() const { return state_; }
  uint8_t mask() const { return mask_; }     // every active/latched condition, for the dashboard
  static Perms perms(HState s);
 private:
  uint8_t latch_, mask_;
  HState state_;
};
}
