#pragma once
#include <stdint.h>
#include "as608_attendance_client.h"
#include "config_store.h"
#include "log_store.h"
#include "roster_store.h"
#include "day_view.h"

namespace att {

struct Occupancy;
class As608Runtime;

// The physical page convention is intentionally injected. Do not replace this with
// page+1 or page identity until delegated decision E18 is explicitly resolved.
class IAs608PageResolver {
 public:
  virtual ~IAs608PageResolver() {}
  virtual bool studentIdForPage(uint16_t physicalPage, uint16_t& studentId) const = 0;
};

// Product-level admission belongs to the serialized application loop. It must reject
// no-class and already-closed sessions. Keeping it explicit prevents this adapter from
// silently inventing those policies or maintaining a stale DayCache.
class IAttendanceAdmission {
 public:
  virtual ~IAttendanceAdmission() {}
  virtual bool allowScan(int32_t localDay, uint8_t session) const = 0;
  // Called after durable commit as part of the same serialized update sequence; the
  // caller immediately applies ClosureEngine::setSchedule() before processing resumes.
  virtual bool canSetParamsAfterCommit(const Params& params) const = 0;
  virtual bool setParamsAfterCommit(const Params& params) = 0;
  // Optional duplicate suppression. A production log-backed policy overrides this.
  virtual bool alreadyScanned(int32_t, uint8_t, uint16_t) const { return false; }
};

// Concrete fail-closed admission backed by the append-only event log. It observes
// no-class and close records incrementally; a reset, log fault, failed replay, or
// observer-capacity failure blocks attendance. One instance must be attached to the
// same LogStore used by the pipeline and remain alive while that LogStore can notify it.
class LogAttendanceAdmission : public IAttendanceAdmission, public LogObserver {
 public:
  LogAttendanceAdmission(LogStore& log, const Params& params);
  LogAttendanceAdmission(const LogAttendanceAdmission&) = delete;
  LogAttendanceAdmission& operator=(const LogAttendanceAdmission&) = delete;
  bool attached() const { return attached_; }
  bool usesLog(const LogStore& log) const { return &log_ == &log; }
  bool usesParams(const Params& params) const {
    return params_.utcOffsetMin == params.utcOffsetMin &&
        params_.lateMin == params.lateMin &&
        params_.scanLeadMin == params.scanLeadMin;
  }
  bool allowScan(int32_t localDay, uint8_t session) const override;
  bool canSetParamsAfterCommit(const Params& params) const override;
  bool setParamsAfterCommit(const Params& params) override;
  bool alreadyScanned(int32_t localDay, uint8_t session, uint16_t studentId) const override;
  void onReset() override;
  void onRecord(const Event& event) override;
 private:
  bool viewFor_(int32_t localDay, uint8_t session) const;
  LogStore& log_;
  Params params_;
  mutable DayView view_;
  mutable int32_t day_;
  mutable uint8_t session_;
  mutable bool viewValid_;
  bool attached_;
};

enum AttendancePipelineResult : uint8_t {
  ATT_PIPE_IDLE,
  ATT_PIPE_PENDING,
  ATT_PIPE_SAVED_PRESENT,
  ATT_PIPE_SAVED_LATE,
  ATT_PIPE_ALREADY_RECORDED,
  ATT_PIPE_NO_FINGER,
  ATT_PIPE_NO_MATCH,
  ATT_PIPE_BAD_IMAGE,
  ATT_PIPE_OUTSIDE_WINDOW,
  ATT_PIPE_NOT_ACTIVE,
  ATT_PIPE_REJECTED,
  ATT_PIPE_NOT_SAVED,          // no durable append was attempted
  ATT_PIPE_SENSOR_FAULT,
  ATT_PIPE_COMMIT_UNCERTAIN     // append returned false; storage may contain none, some or all bytes
};

// Converts a completed physical sensor match into one durable EV_SCAN event.
// Calls are serialized with ClosureEngine/config changes on the application loop.
// The pipeline owns no sensor resources; As608AttendanceClient owns the full scan lease.
class As608AttendancePipeline {
 public:
  // Preferred runtime-bound construction: raw attendance-client mutation remains
  // private to this pipeline and the startup coordinator.
  As608AttendancePipeline(As608Runtime& runtime, LogStore& log,
      RosterStore& roster, const Config& config,
      const IAs608PageResolver& pages, IAttendanceAdmission& admission);
  // Low-level client-bound construction remains for isolated tests/adapters.
  As608AttendancePipeline(As608AttendanceClient& sensor, LogStore& log,
      RosterStore& roster, const Occupancy& occupancy, const Config& config,
      const IAs608PageResolver& pages, IAttendanceAdmission& admission);
  As608AttendancePipeline(const As608AttendancePipeline&) = delete;
  As608AttendancePipeline& operator=(const As608AttendancePipeline&) = delete;

  // Call after the durable commit, before ClosureEngine::setSchedule(), in the same
  // serialized config-apply operation. Do not run step() between these updates.
  bool canSetConfigAfterCommit(const Config& config) const;
  bool setConfigAfterCommit(const Config& config);
  bool begin(uint32_t nowEpoch);
  AttendancePipelineResult step();
  bool active() const { return active_; }

 private:
  AttendancePipelineResult saveMatch_(uint16_t physicalPage, uint16_t score);
  As608AttendanceClient& sensor_;
  LogStore& log_;
  RosterStore& roster_;
  const Occupancy& occupancy_;
  Config config_;
  const IAs608PageResolver& pages_;
  IAttendanceAdmission& admission_;
  uint32_t scanEpoch_;
  bool active_;
};

}  // namespace att
