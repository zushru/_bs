#include "health_fsm.h"
namespace att {
HState HealthFsm::update(const HealthInputs& in) {
  uint8_t active = 0;
  if (!in.storageOk) active |= HB_STORAGE;
  if (!in.cfgOk) active |= HB_CONFIG;
  if (in.rtcLostPower || !in.rtcPresent || in.epochBelowFloor || in.epochBelowLog) active |= HB_CLOCK;
  latch_ |= (uint8_t)(active & (HB_STORAGE | HB_CONFIG | HB_CLOCK));          // faults latch...
  if (in.reverified) latch_ &= active;                                          // ...until cause gone AND re-verified
  uint8_t m = (uint8_t)(latch_ | active);
  if (!in.provisioned) m |= HB_UNPROV;
  if (!in.sensorOk) m |= HB_SENSOR;
  mask_ = m;
  if (m & HB_STORAGE) state_ = H_STORAGE_FAULT;
  else if (m & HB_CONFIG) state_ = H_CONFIG_FAULT;
  else if (m & HB_UNPROV) state_ = H_UNPROVISIONED;
  else if (m & HB_CLOCK) state_ = H_CLOCK_FAULT;
  else if (m & HB_SENSOR) state_ = H_SENSOR_FAULT;
  else state_ = H_OK;
  return state_;
}
Perms HealthFsm::perms(HState s) {
  switch (s) {
    case H_OK:            return Perms{true, true, true, true, true};
    case H_SENSOR_FAULT:  return Perms{false, true, false, true, true};
    case H_CONFIG_FAULT:  return Perms{false, false, false, true, true};    // raw log only
    case H_CLOCK_FAULT:   return Perms{false, false, false, true, false};   // history flagged, clock set only
    case H_STORAGE_FAULT: return Perms{false, false, false, false, false};  // status only
    default:              return Perms{false, false, false, false, false};  // unprovisioned: wizard only
  }
}
}
