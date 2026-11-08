#include "pin_auth_session.h"
#include <string.h>

namespace att {
namespace {
void secureClear(uint8_t* value, size_t length) {
  volatile uint8_t* p = value;
  while (length--) *p++ = 0;
}
}  // namespace

TokenSessionSuccessAction::TokenSessionSuccessAction(
    TokenTable& sessions, ISecureRandomSource& random)
    : sessions_(sessions), random_(random), token_{0},
      attempted_(false), issued_(false), copied_(false), committed_(false),
      evicted_(false) {}

TokenSessionSuccessAction::~TokenSessionSuccessAction() { abort(); }

PinActionOutcome TokenSessionSuccessAction::run() {
  if (attempted_ || committed_) return PA_INDETERMINATE;
  attempted_ = true;
  secureClear(token_, sizeof token_);

  if (!random_.fill(token_, sizeof token_)) {
    secureClear(token_, sizeof token_);
    return PA_INDETERMINATE;
  }

  // Read-only preflight rejects all-zero and duplicate tokens without evicting
  // an existing session before the response is actually delivered.
  if (!sessions_.canIssue(token_)) {
    secureClear(token_, sizeof token_);
    return PA_INDETERMINATE;
  }

  issued_ = true;  // pending delivery; not active in TokenTable yet
  evicted_ = false;
  return PA_DONE;
}

bool TokenSessionSuccessAction::takeToken(uint8_t out[16]) {
  if (out == nullptr || !issued_ || copied_ || committed_) return false;
  memcpy(out, token_, sizeof token_);
  copied_ = true;
  return true;
}

bool TokenSessionSuccessAction::confirmResponseSent(uint32_t nowS) {
  if (!issued_ || !copied_ || committed_) return false;
  // This action is synchronous/serialized on the device main loop. No other
  // TokenTable mutation may run between run() preflight and this commit.
  if (!sessions_.issue(token_, nowS)) {
    issued_ = false;
    secureClear(token_, sizeof token_);
    return false;
  }
  evicted_ = sessions_.lastIssueEvicted();
  committed_ = true;
  issued_ = false;
  secureClear(token_, sizeof token_);
  return true;
}

void TokenSessionSuccessAction::abort() {
  // run() performs only a read-only TokenTable preflight. Nothing is inserted
  // until confirmResponseSent(nowS), so abort never revokes or evicts a session.
  issued_ = false;
  secureClear(token_, sizeof token_);
}

}  // namespace att
