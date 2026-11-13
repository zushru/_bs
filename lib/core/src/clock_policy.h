#pragma once
#include "log_store.h"
#include "time_rules.h"
namespace att {
const int64_t CLK_INFO_S = 120, CLK_AUTO_MAX_S = 300, CLK_JUMP_S = 86400, CLK_AUTO_EVERY_S = 86400;
enum ClockAction : uint8_t { CA_NONE = 0, CA_APPLY, CA_NEED_CONFIRM_BACK, CA_NEED_CONFIRM_JUMP, CA_BLOCKED_WINDOW };
struct ClockCtx {
  uint32_t deviceEpoch, clientEpoch;
  uint32_t lastLogEpoch;      // LogStore baseline, 0 if log empty
  uint32_t lastAutoSync;      // epoch of last auto-sync, 0 = never
  bool clockFault;            // CLOCK_FAULT or UNPROVISIONED: device time is not trusted
  bool explicitSync, confirmBack, confirmJump, autoSyncEnabled;
  bool inWindow;              // inside a scan window [start-lead, end)
  bool nearWindow;            // inside a window or within 5 min before one
};
struct ClockDecision {
  ClockAction action; int64_t delta; uint16_t source; bool needsAttention;
  uint32_t gapFrom, gapTo;    // gapTo > gapFrom => a GAP must be written with the CLOCK_SET
};
ClockDecision decideClock(const ClockCtx& c);
bool nearScanWindow(const Timetable& t, const Params& p, uint32_t epoch, uint32_t marginS);
enum ClockApplyResult : uint8_t {
  CLOCK_APPLY_REJECTED = 0,          // decision/input invalid; no append attempted
  CLOCK_APPLY_COMMITTED,             // GAP coverage (if any) and CLOCK_SET pair appended
  CLOCK_APPLY_COMMIT_UNCERTAIN,      // one or more appends failed; storage may contain a prefix or complete commit
  CLOCK_APPLY_LOG_FAULTED            // log was already faulted; no append attempted
};
// Only CA_APPLY decisions with a valid source and ordered gap bounds may write.
// COMMIT_UNCERTAIN requires the caller to stop writes and reopen/reconcile before retrying.
ClockApplyResult applyClockSet(LogStore& log, const ClockDecision& d,
                               uint32_t oldEpoch, uint32_t newEpoch);
}
