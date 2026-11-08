#pragma once

#include <stddef.h>
#include <stdint.h>
#include "pin_auth.h"

namespace att {

// Cryptographically secure byte source. Implementations must refuse to fill
// before a continuous entropy source is available; a bootloader-seeded PRNG
// stream is not sufficient for network session tokens.
class ISecureRandomSource {
 public:
  virtual ~ISecureRandomSource() {}
  virtual bool fill(uint8_t* out, size_t length) = 0;
};

// Success action that bridges PinAuthCoordinator to the existing RAM-only
// TokenTable. run() preflights a random token without changing active sessions.
// The caller must copy it into the HTTP response and call confirmResponseSent(nowS)
// only after the entire response write succeeds; insertion/LRU eviction occurs
// at that point. The caller must not yield to another TokenTable mutation between
// run() and confirmation. If the response is abandoned, destruction/abort clears
// the pending token without changing existing sessions. Serialized-main-loop use.
class TokenSessionSuccessAction : public IPinSuccessAction {
 public:
  TokenSessionSuccessAction(TokenTable& sessions, ISecureRandomSource& random);
  ~TokenSessionSuccessAction() override;

  TokenSessionSuccessAction(const TokenSessionSuccessAction&) = delete;
  TokenSessionSuccessAction& operator=(const TokenSessionSuccessAction&) = delete;

  PinActionOutcome run() override;

  // Exactly one response copy is allowed. The response owner must erase its
  // temporary token copy after serialization/transmission.
  bool takeToken(uint8_t out[16]);

  // Commit delivery only after the complete response containing the token was
  // successfully written. Pass monotonic seconds at the moment of confirmation
  // so the idle expiry starts at actual token activation, not request start.
  bool confirmResponseSent(uint32_t nowS);

  // Explicitly discard an uncommitted token. Safe to call more than once.
  void abort();

  bool evictedExistingSession() const { return evicted_; }  // meaningful after confirmation
  bool pendingDelivery() const { return issued_ && !committed_; }

 private:
  TokenTable& sessions_;
  ISecureRandomSource& random_;
  uint8_t token_[16];
  bool attempted_;
  bool issued_;
  bool copied_;
  bool committed_;
  bool evicted_;
};

}  // namespace att
