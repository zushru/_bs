#pragma once
#include "config_provision.h"

namespace att {

// The production boot path must not construct runtime services from defaults when
// NVS is blank or unhealthy. This interface combines the slot backend with the
// explicit begin/load lifecycle provided by the platform adapter.
class IConfigBootBackend : public IConfigProvisionBackend {
 public:
  virtual bool begin() = 0;
  virtual bool load(Config& out, CfgPick& pick) = 0;
};

enum ConfigBootState : uint8_t {
  CFG_BOOT_COLD,
  CFG_BOOT_READY,
  CFG_BOOT_NEEDS_SETUP,
  CFG_BOOT_FAULT
};

enum ConfigBootCode : uint8_t {
  CFG_BOOT_OK,
  CFG_BOOT_SETUP_REQUIRED,
  CFG_BOOT_ALREADY_STARTED,
  CFG_BOOT_BACKEND_BEGIN_FAILED,
  CFG_BOOT_LOAD_FAILED,
  CFG_BOOT_INVALID_SELECTION,
  CFG_BOOT_NOT_SETUP_REQUIRED,
  CFG_BOOT_PROVISION_REJECTED,
  CFG_BOOT_PROVISION_FAILED,
  CFG_BOOT_RELOAD_FAILED
};

struct ConfigBootResult {
  ConfigBootCode code;
  ConfigBootState state;
  ConfigProvisionCode provisionCode;  // meaningful only for provision results
};

// One-shot boot/provision coordinator. It is deliberately constructed before the
// rest of the application runtime. Runtime services may be constructed only after
// state() == CFG_BOOT_READY and config()/selection() are valid.
//
// All calls are serialized on the main loop. Any storage/verification ambiguity
// latches CFG_BOOT_FAULT for this boot; reboot and a fresh backend load are required.
class ConfigBootCoordinator {
 public:
  explicit ConfigBootCoordinator(IConfigBootBackend& backend);
  ConfigBootCoordinator(const ConfigBootCoordinator&) = delete;
  ConfigBootCoordinator& operator=(const ConfigBootCoordinator&) = delete;

  ConfigBootResult start();
  ConfigBootResult provision(const Config& initial, const IConfigProvisionGate& gate);

  ConfigBootState state() const { return state_; }
  bool ready() const { return state_ == CFG_BOOT_READY; }
  bool needsSetup() const { return state_ == CFG_BOOT_NEEDS_SETUP; }
  bool faulted() const { return state_ == CFG_BOOT_FAULT; }
  const Config* config() const { return ready() ? &config_ : nullptr; }
  bool selection(CfgPick& out) const;

 private:
  ConfigBootResult result_(ConfigBootCode code,
                           ConfigProvisionCode provisionCode = CFG_PROVISION_NOT_UNPROVISIONED) const;
  void latchFault_();

  IConfigBootBackend& backend_;
  ConfigBootState state_;
  Config config_;
  CfgPick selected_;
};

}  // namespace att
