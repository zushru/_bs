#pragma once
#include "as608_attendance_owner.h"
#include "config_apply.h"
#include "closure_engine.h"

namespace att {

// Bridges the preferred owned sensor composition to ConfigApplyCoordinator.
// ConfigApplyCoordinator commits and verifies durable storage before applyAfterCommit().
// All calls must share the same serialized application loop as attendance and closure work.
class As608OwnerConfigRuntime : public IConfigApplyRuntime {
 public:
  As608OwnerConfigRuntime(As608AttendanceOwner& owner, ClosureEngine& closure)
      : owner_(owner), closure_(closure),
        bindingsValid_(owner.matchesClosure(closure)) {}
  As608OwnerConfigRuntime(const As608OwnerConfigRuntime&) = delete;
  As608OwnerConfigRuntime& operator=(const As608OwnerConfigRuntime&) = delete;

  bool canApply(const Config& next) const override;
  bool applyAfterCommit(const Config& next) override;

 private:
  As608AttendanceOwner& owner_;
  ClosureEngine& closure_;
  bool bindingsValid_;
};

}  // namespace att
