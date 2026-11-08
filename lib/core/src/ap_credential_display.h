#pragma once

#include <stddef.h>
#include "ap_credential_bootstrap.h"

namespace att {

// Adapter port for disclosing and clearing a first-boot AP credential.
// A successful return means a transfer was requested, not that pixels were
// physically visible or acknowledged by the display controller.
class IApCredentialDisplay {
 public:
  virtual ~IApCredentialDisplay() {}
  virtual bool requestCredentialDisplay(const ApCredentialResult& result,
                                        const char* passphrase,
                                        size_t passphraseLength) = 0;
  virtual bool requestBlankFrame() = 0;
};

}  // namespace att
