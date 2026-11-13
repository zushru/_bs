#include "admin_clock_confirmation.h"

namespace att {
namespace {

ClockConfirmResult notInvokedResult() {
  const ClockConfirmResult value = {
      CCF_BAD_INPUT, CA_NONE, CLOCK_APPLY_REJECTED, 0, 0, 0, false};
  return value;
}

}  // namespace

AdminClockDispatchResult AdminClockConfirmation::confirm(
    const uint8_t requestToken[16], uint32_t nowS, LogStore& log,
    MonthSet& months, const ClockConfirmRequest& request) {
  const ApiResult authorization = adminSessions_.authorize(
      EP_CLOCK, clock_.healthState(), requestToken, nowS);

  if (authorization.code != A_OK) {
    const AdminClockDispatchResult denied = {
        ACDC_AUTHORIZATION_DENIED, authorization, notInvokedResult()};
    return denied;
  }

  const ClockConfirmResult confirmation = clock_.confirm(
      log, months, request);
  const AdminClockDispatchResult invoked = {
      ACDC_CONFIRMATION_INVOKED, authorization, confirmation};
  return invoked;
}

}  // namespace att
