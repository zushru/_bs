#include "clock_confirmation.h"

namespace att {
namespace {

ClockConfirmResult result(ClockConfirmCode code, ClockAction action,
                          ClockApplyResult applied, uint32_t previousEpoch,
                          uint32_t requestedEpoch, uint32_t baseline,
                          bool trusted) {
  ClockConfirmResult value = {
      code, action, applied, previousEpoch, requestedEpoch, baseline, trusted};
  return value;
}

bool rtcSampleWithinWriteWindow(uint32_t requestedEpoch,
                                uint32_t observedEpoch,
                                uint64_t writeStartedUs,
                                uint64_t observedAtUs) {
  if (observedAtUs < writeStartedUs || observedEpoch < requestedEpoch)
    return false;
  const uint64_t elapsedUs = observedAtUs - writeStartedUs;
  const uint64_t maximumAdvanceSeconds = elapsedUs / 1000000ULL +
      (elapsedUs % 1000000ULL != 0 ? 1ULL : 0ULL);
  return static_cast<uint64_t>(observedEpoch - requestedEpoch) <=
      maximumAdvanceSeconds;
}

bool clockPairAtEnd(LogStore& log, size_t oldCount, uint16_t source,
                    uint32_t oldEpoch, uint32_t newEpoch) {
  const size_t count = log.count();
  if (count < 2 || count < oldCount || count - oldCount < 2) return false;

  Event oldEvent = {};
  Event newEvent = {};
  if (!log.at(count - 2, oldEvent) || !log.at(count - 1, newEvent))
    return false;

  return oldEvent.type == EV_CLOCK_SET &&
      newEvent.type == EV_CLOCK_SET &&
      oldEvent.a == source && newEvent.a == source &&
      oldEvent.epoch == oldEpoch && newEvent.epoch == newEpoch &&
      flagMore(oldEvent.flags) && !flagMore(newEvent.flags) &&
      oldEvent.seq != UINT32_MAX && newEvent.seq == oldEvent.seq + 1u &&
      newEvent.seq == log.lastSeq() &&
      log.baselineEpoch() == newEpoch;
}

}  // namespace

ClockConfirmResult ClockConfirmationService::confirm(
    LogStore& log, MonthSet& months,
    const ClockConfirmRequest& request) {
  const ClockApplyResult notApplied = CLOCK_APPLY_REJECTED;
  if (blocked_)
    return result(CCF_SERVICE_LATCHED, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);

  const HState healthState = health_.state();
  if (healthState != H_OK && healthState != H_CLOCK_FAULT &&
      healthState != H_SENSOR_FAULT)
    return result(CCF_HEALTH_BLOCKED, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);
  const bool currentClockFault = healthState == H_CLOCK_FAULT;

  const Config* config = configBoot_.config();
  if (!configBoot_.ready() || config == nullptr)
    return result(CCF_CONFIG_NOT_READY, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);
  const Params& params = config->params;
  const Timetable& timetable = config->tt;

  if (storageOwner_.state() != LITTLEFS_OWNER_READY)
    return result(CCF_STORAGE_OWNER_NOT_READY, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);
  IFileSet* ownerFiles = storageOwner_.fileSet();
  if (!ownerFiles)
    return result(CCF_STORAGE_OWNER_NOT_READY, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);
  if (!months.usesFileSet(*ownerFiles))
    return result(CCF_STORAGE_OWNER_MISMATCH, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);

  if (log.faulted())
    return result(CCF_LOG_NOT_READY, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);
  if (!log.usesStorage(months))
    return result(CCF_STORAGE_MISMATCH, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);

  uint32_t requestedMonthKey = 0;
  if (!validateConfig(*config) ||
      request.requestedUtcEpoch < EPOCH_2020 ||
      params.utcOffsetMin < -720 || params.utcOffsetMin > 840 ||
      !monthKeyForEpoch(request.requestedUtcEpoch, params.utcOffsetMin,
                        requestedMonthKey) ||
      !validateTimetable(timetable, params.scanLeadMin))
    return result(CCF_BAD_INPUT, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, log.baselineEpoch(), false);

  const uint32_t baseline = log.baselineEpoch();
  Ds3231Reading before = {0, false};
  const Ds3231Code readBefore = rtc_.readUtcEpoch(0, before);
  const bool rtcCalendarValid = readBefore == DS3231_OK;
  if (readBefore != DS3231_OK && readBefore != DS3231_INVALID_TIME)
    return result(CCF_RTC_READ_FAILED, CA_NONE, notApplied, 0,
                  request.requestedUtcEpoch, baseline, false);

  const bool belowLog = rtcCalendarValid && baseline != 0 &&
      before.epochUtc < baseline;
  const bool policyClockFault = currentClockFault || !rtcCalendarValid ||
      (rtcCalendarValid && (before.oscillatorStopped ||
       before.epochUtc < EPOCH_2020 || belowLog));
  const uint32_t deviceEpoch = rtcCalendarValid ? before.epochUtc : 0;

  bool inWindow = false;
  bool nearWindow = false;
  if (!policyClockFault) {
    inWindow = sessionAt(timetable, params, deviceEpoch) >= 0;
    const uint32_t margin = 300u;
    const uint32_t laterEpoch = deviceEpoch > UINT32_MAX - margin
        ? UINT32_MAX : deviceEpoch + margin;
    nearWindow = inWindow ||
        sessionAt(timetable, params, laterEpoch) >= 0;
  }

  const ClockCtx context = {
      deviceEpoch,
      request.requestedUtcEpoch,
      baseline,
      0,
      policyClockFault,
      true,
      request.confirmBackward,
      request.confirmJump,
      false,
      inWindow,
      nearWindow};
  const ClockDecision decision = decideClock(context);
  const uint32_t oldEpoch = policyClockFault ? baseline : deviceEpoch;

  if (decision.action == CA_NEED_CONFIRM_BACK ||
      decision.action == CA_NEED_CONFIRM_JUMP)
    return result(CCF_CONFIRMATION_REQUIRED, decision.action, notApplied,
                  oldEpoch, request.requestedUtcEpoch, baseline, false);
  if (decision.action == CA_BLOCKED_WINDOW)
    return result(CCF_BLOCKED_WINDOW, decision.action, notApplied, oldEpoch,
                  request.requestedUtcEpoch, baseline, false);
  if (decision.action != CA_APPLY || decision.source > SRC_WIZARD ||
      !gapSpanEncodable(decision.gapFrom, decision.gapTo))
    return result(CCF_POLICY_REJECTED, decision.action, notApplied, oldEpoch,
                  request.requestedUtcEpoch, baseline, false);

  // Select the exact local month derived from the requested UTC timestamp.
  // MonthSet preserves append-only order if this key is older than its newest
  // existing file; it creates a new month only when the key moves forward.
  months.setMonth(requestedMonthKey);

  // Commit and replay the audit intent before changing the physical clock.
  // If the log is torn or only a GAP prefix survives, do not touch the RTC.
  const size_t oldCount = log.count();
  const ClockApplyResult applied =
      applyClockSet(log, decision, oldEpoch, request.requestedUtcEpoch);
  if (applied == CLOCK_APPLY_REJECTED) {
    blocked_ = true;
    return result(CCF_LOG_APPEND_REJECTED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, baseline, false);
  }
  if (applied == CLOCK_APPLY_LOG_FAULTED) {
    blocked_ = true;
    return result(CCF_LOG_WAS_FAULTED, decision.action, applied, oldEpoch,
                  request.requestedUtcEpoch, baseline, false);
  }

  // Reopen after both COMMITTED and COMMIT_UNCERTAIN. A complete pair that
  // survived an uncertain return is reconciled; no logical write is retried.
  const OpenInfo replay = log.open();
  if (!replay.ok) {
    blocked_ = true;
    return result(CCF_LOG_REPLAY_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, log.baselineEpoch(),
                  false);
  }
  if (!clockPairAtEnd(log, oldCount, decision.source, oldEpoch,
                      request.requestedUtcEpoch)) {
    blocked_ = true;
    return result(CCF_LOG_COMMIT_INCOMPLETE, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }

  // The audit pair is now durable and replay-verified. Only now write UTC.
  // Any subsequent RTC failure leaves the committed intent visible and this
  // service latched; the caller must keep attendance disabled.
  uint64_t writeStartedUs = 0;
  if (!monotonic_.nowMicros(writeStartedUs)) {
    blocked_ = true;
    return result(CCF_MONOTONIC_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }
  const Ds3231Code writeCode =
      rtc_.writeUtcEpoch(request.requestedUtcEpoch, 0);
  if (writeCode == DS3231_IO_NOT_STARTED) {
    blocked_ = true;
    return result(CCF_RTC_WRITE_NOT_STARTED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }
  if (writeCode == DS3231_INVALID_ARGUMENT ||
      writeCode == DS3231_INVALID_TIME) {
    blocked_ = true;
    return result(CCF_RTC_WRITE_REJECTED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }

  Ds3231Reading afterWrite = {0, false};
  const Ds3231Code readAfterWrite = rtc_.readUtcEpoch(0, afterWrite);
  if (readAfterWrite != DS3231_OK) {
    blocked_ = true;
    return result(writeCode == DS3231_IO_UNCERTAIN
                      ? CCF_RTC_WRITE_UNCERTAIN : CCF_RTC_VERIFY_FAILED,
                  decision.action, applied, oldEpoch,
                  request.requestedUtcEpoch, replay.baselineEpoch, false);
  }
  uint64_t afterWriteUs = 0;
  if (!monotonic_.nowMicros(afterWriteUs) ||
      afterWriteUs < writeStartedUs) {
    blocked_ = true;
    return result(CCF_MONOTONIC_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }
  if (!rtcSampleWithinWriteWindow(request.requestedUtcEpoch,
                                  afterWrite.epochUtc,
                                  writeStartedUs, afterWriteUs)) {
    blocked_ = true;
    return result(writeCode == DS3231_IO_UNCERTAIN
                      ? CCF_RTC_WRITE_UNCERTAIN : CCF_RTC_VERIFY_FAILED,
                  decision.action, applied, oldEpoch,
                  request.requestedUtcEpoch, replay.baselineEpoch, false);
  }

  // Clear OSF only after a calendar sample consistent with elapsed monotonic
  // time. Always re-read the complete calendar/status pair afterward so an
  // uncertain flag clear is reconciled.
  (void)rtc_.clearOscillatorStopFlag();
  Ds3231Reading afterClear = {0, false};
  const Ds3231Code readAfterClear = rtc_.readUtcEpoch(0, afterClear);
  if (readAfterClear != DS3231_OK || afterClear.oscillatorStopped) {
    blocked_ = true;
    return result(CCF_OSF_CLEAR_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }
  uint64_t afterClearUs = 0;
  if (!monotonic_.nowMicros(afterClearUs) ||
      afterClearUs < afterWriteUs) {
    blocked_ = true;
    return result(CCF_MONOTONIC_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }
  if (!rtcSampleWithinWriteWindow(request.requestedUtcEpoch,
                                  afterClear.epochUtc,
                                  writeStartedUs, afterClearUs)) {
    blocked_ = true;
    return result(CCF_OSF_CLEAR_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }

  // Final cross-check: the replayed intent and current UTC calendar/OSF agree.
  Ds3231Reading finalRtc = {0, false};
  if (rtc_.readUtcEpoch(0, finalRtc) != DS3231_OK ||
      finalRtc.oscillatorStopped || log.faulted() ||
      log.baselineEpoch() != request.requestedUtcEpoch) {
    blocked_ = true;
    return result(CCF_RTC_VERIFY_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, log.baselineEpoch(),
                  false);
  }
  uint64_t finalReadUs = 0;
  if (!monotonic_.nowMicros(finalReadUs) ||
      finalReadUs < afterClearUs) {
    blocked_ = true;
    return result(CCF_MONOTONIC_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, replay.baselineEpoch,
                  false);
  }
  if (!rtcSampleWithinWriteWindow(request.requestedUtcEpoch,
                                  finalRtc.epochUtc,
                                  writeStartedUs, finalReadUs)) {
    blocked_ = true;
    return result(CCF_RTC_VERIFY_FAILED, decision.action, applied,
                  oldEpoch, request.requestedUtcEpoch, log.baselineEpoch(),
                  false);
  }

  return result(CCF_OK, decision.action, applied, oldEpoch,
                request.requestedUtcEpoch, log.baselineEpoch(), true);
}

}  // namespace att
