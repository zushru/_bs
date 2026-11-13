#pragma once
#include <stdint.h>
#include "as608_startup.h"
#include "application_attendance_gate.h"

namespace att {

// Preferred attendance-only composition for the future production root. Owns one
// As608Runtime (including its occupancy map), its startup coordinator and checked pipeline.
// Do not construct multiple owners for one physical sensor. All calls must be made
// from one serialized application loop; this class does not add an RTOS mutex.
//
// LogStore, RosterStore, page resolver and admission policy are non-owning dependencies
// and must outlive this owner. Enrollment is intentionally not exposed here: E18 page
// mapping and a production enrollment workflow remain unresolved.
class As608AttendanceOwner {
 public:
  As608AttendanceOwner(Stream& stream, LogStore& log, RosterStore& roster,
      const Config& config, const IAs608PageResolver& pages,
      ApplicationAttendanceGate& gate, uint32_t sensorPassword);
  As608AttendanceOwner(const As608AttendanceOwner&) = delete;
  As608AttendanceOwner& operator=(const As608AttendanceOwner&) = delete;

  // Call only after storage/config boot gates are complete.
  bool beginStartup();
  As608StartupStatus stepStartup();
  bool startupReady() const { return startup_.ready(); }
  bool startupFaulted() const { return startup_.faulted(); }
  bool matchesClosure(const ClosureEngine& closure) const {
    return gate_.matchesClosure(closure);
  }
  uint16_t sensorCapacity() const { return startup_.capacity(); }
  bool transportFaulted() const { return runtime_.transportFaulted(); }
  const Occupancy& occupancy() const { return runtime_.occupancy(); }

  // The only public attendance operation path; raw sensor client access is not exposed.
  bool beginScan(uint32_t nowEpoch);
  AttendancePipelineResult stepScan();
  bool scanActive() const { return pipeline_.active(); }

  // Preserve the existing config-apply ordering contract: durable config commit first,
  // then update admission/pipeline here, then update ClosureEngine before more steps.
  bool canSetConfigAfterCommit(const Config& config) const;
  bool setConfigAfterCommit(const Config& config);

 private:
  ApplicationAttendanceGate& gate_;
  bool bindingsValid_;
  As608Runtime runtime_;
  As608StartupCoordinator startup_;
  As608AttendancePipeline pipeline_;
};

}  // namespace att
