#include "as608_attendance_pipeline.h"
namespace att {

LogAttendanceAdmission::LogAttendanceAdmission(LogStore& log, const Params& params)
    : log_(log), params_(params), day_(-1), session_(0), viewValid_(false),
      attached_(false) {
  attached_ = log_.attach(this);
}

void LogAttendanceAdmission::onReset() {
  viewValid_ = false;
  day_ = -1;
  session_ = 0;
}

void LogAttendanceAdmission::onRecord(const Event& event) {
  if (viewValid_) applyEvent(view_, event, params_);
}

bool LogAttendanceAdmission::canSetParamsAfterCommit(const Params& params) const {
  return attached_ && !log_.faulted() &&
      params.utcOffsetMin >= -720 && params.utcOffsetMin <= 840 &&
      params.scanLeadMin <= 60 && params.lateMin <= 240;
}

bool LogAttendanceAdmission::setParamsAfterCommit(const Params& params) {
  if (!canSetParamsAfterCommit(params)) return false;
  params_ = params;
  viewValid_ = false;
  day_ = -1;
  session_ = 0;
  return true;
}

bool LogAttendanceAdmission::viewFor_(int32_t localDay, uint8_t session) const {
  if (!attached_ || log_.faulted() || localDay < 0 || session > 1) return false;
  if (!viewValid_ || day_ != localDay || session_ != session) {
    viewValid_ = false;
    if (!buildDayView(log_, params_, localDay, session, view_)) return false;
    day_ = localDay;
    session_ = session;
    viewValid_ = true;
  }
  return true;
}

bool LogAttendanceAdmission::allowScan(int32_t localDay, uint8_t session) const {
  if (!viewFor_(localDay, session)) return false;
  return !view_.noClassDay && !view_.noClassSession && !view_.closed;
}

bool LogAttendanceAdmission::alreadyScanned(int32_t localDay, uint8_t session,
                                            uint16_t studentId) const {
  if (studentId < 1 || studentId > MAX_ID || !viewFor_(localDay, session)) return false;
  const uint8_t status = view_.scan[studentId];
  return status == ST_PRESENT || status == ST_LATE;
}

}  // namespace att
