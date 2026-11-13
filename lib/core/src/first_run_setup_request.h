#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config_store.h"

namespace att {

static const size_t FIRST_RUN_SETUP_REQUEST_MAX_BYTES = 768;

enum FirstRunSetupRequestCode : uint8_t {
  FRSR_OK,
  FRSR_INVALID_REQUEST
};

struct FirstRunSetupRequest {
  char setupCode[7];
  char initialPin[7];
  Config config;
};

enum FirstRunSetupBodyCode : uint8_t {
  FRSB_OK,
  FRSB_BAD_STATE,
  FRSB_INVALID_LENGTH,
  FRSB_INVALID_CHUNK,
  FRSB_TOO_LARGE,
  FRSB_INCOMPLETE
};

// Fixed-capacity accumulator for the WebServer raw-body callback contract.
// totalSize passed to append() is the cumulative byte count after this chunk.
// The body is exposed only after finish() confirms the declared length exactly.
class FirstRunSetupBodyBuffer {
 public:
  FirstRunSetupBodyBuffer();
  ~FirstRunSetupBodyBuffer();

  FirstRunSetupBodyBuffer(const FirstRunSetupBodyBuffer&) = delete;
  FirstRunSetupBodyBuffer& operator=(const FirstRunSetupBodyBuffer&) = delete;

  FirstRunSetupBodyCode begin(size_t expectedLength);
  FirstRunSetupBodyCode append(const uint8_t* chunk, size_t currentSize,
                               size_t totalSize);
  FirstRunSetupBodyCode finish();
  void abort();
  void clear();

  bool ready() const { return ready_; }
  const char* data() const { return ready_ ? bytes_ : nullptr; }
  size_t length() const { return ready_ ? bodyLength_ : 0; }

 private:
  char bytes_[FIRST_RUN_SETUP_REQUEST_MAX_BYTES + 1];
  size_t bodyLength_;
  size_t expectedLength_;
  bool receiving_;
  bool ready_;
};

// Strict schema decoder. It accepts only the documented first-run JSON shape;
// unknown/duplicate/missing keys, escapes, non-integer numbers and trailing
// data are rejected. Output is unchanged on failure.
FirstRunSetupRequestCode decodeFirstRunSetupRequest(
    const char* json, size_t length, FirstRunSetupRequest& out);
void clearFirstRunSetupRequest(FirstRunSetupRequest& request);

}  // namespace att
