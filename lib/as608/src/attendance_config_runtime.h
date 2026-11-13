#pragma once
#include "config_apply.h"
#include "as608_attendance_pipeline.h"
#include "closure_engine.h"

namespace att {

// Applies one already-persisted Config to the live attendance pipeline and closure
// engine. Calls are single-threaded and serialized by ConfigApplyCoordinator.
class AttendanceConfigRuntime : public IConfigApplyRuntime {
 public:
  AttendanceConfigRuntime(As608AttendancePipeline& pipeline, ClosureEngine& closure)
      : pipeline_(pipeline), closure_(closure) {}
  bool canApply(const Config& next) const override;
  bool applyAfterCommit(const Config& next) override;
 private:
  As608AttendancePipeline& pipeline_;
  ClosureEngine& closure_;
};

}  // namespace att
