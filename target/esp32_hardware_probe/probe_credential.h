#pragma once

#include <stddef.h>
#include <stdint.h>

namespace att {

// Encode exactly 128 bits into the 32 lowercase-hex characters accepted by
// Esp32Oled and the AP credential policy. This helper is probe-only: it does
// not persist or select a production credential.
inline bool encodeProbeCredential(const uint8_t* entropy, size_t entropyLength,
                                  char* output, size_t outputLength) {
  if (output != nullptr) {
    for (size_t i = 0; i < outputLength; ++i) output[i] = '\0';
  }
  if (entropy == nullptr || entropyLength != 16 || output == nullptr ||
      outputLength < 33) {
    return false;
  }

  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < 16; ++i) {
    output[i * 2] = kHex[(entropy[i] >> 4) & 0x0F];
    output[i * 2 + 1] = kHex[entropy[i] & 0x0F];
  }
  output[32] = '\0';
  return true;
}

}  // namespace att
