#pragma once
#include "config_apply.h"

namespace att {

// Implemented by the setup/health layer. Return true only after the caller has
// authenticated the one-time setup flow and confirmed that provisioning is allowed.
class IConfigProvisionGate {
 public:
  virtual ~IConfigProvisionGate() {}
  virtual bool mayProvision() const = 0;
};

enum ConfigProvisionCode : uint8_t {
  CFG_PROVISION_OK,
  CFG_PROVISION_NOT_AUTHORIZED,
  CFG_PROVISION_NOT_UNPROVISIONED,
  CFG_PROVISION_INVALID_CONFIG,
  CFG_PROVISION_READ_FAILED,
  CFG_PROVISION_STORAGE_INDETERMINATE,
  CFG_PROVISION_VERIFY_FAILED,
  CFG_PROVISION_FAULTED
};

struct ConfigProvisionResult {
  ConfigProvisionCode code;
  CfgPick selected;
};

// Provision-only backend capability. Ordinary A/B config backends do not need
// to expose marker reads; only the first-provision path requires this stronger
// contract.
class IConfigProvisionBackend : public IConfigSlotBackend {
 public:
  // true means the lookup completed; present=false means no marker exists.
  // Malformed marker data and storage errors must return false.
  virtual bool readProvisionMarker(bool& present) = 0;
};

// One-shot first-config writer. Construct only after backend.begin() and
// backend.load() returned CB_UNPROVISIONED. This class does not implement setup
// code/PIN security; IConfigProvisionGate is a mandatory caller-owned boundary.
// Calls are main-loop-only. Any ambiguous write or post-write verification fault
// latches this object; reboot and reload are required before another attempt.
class ConfigProvisioner {
 public:
  ConfigProvisioner(IConfigProvisionBackend& backend, const IConfigProvisionGate& gate)
      : backend_(backend), gate_(gate), faulted_(false) {}

  ConfigProvisionResult provision(const Config& initial, const CfgPick& loaded);
  bool faulted() const { return faulted_; }

 private:
  IConfigProvisionBackend& backend_;
  const IConfigProvisionGate& gate_;
  bool faulted_;
};

}  // namespace att
