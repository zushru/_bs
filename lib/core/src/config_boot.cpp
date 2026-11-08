#include "config_boot.h"

namespace att {

ConfigBootCoordinator::ConfigBootCoordinator(IConfigBootBackend& backend)
    : backend_(backend), state_(CFG_BOOT_COLD),
      selected_{CB_FAULT, -1, 0} {}

ConfigBootResult ConfigBootCoordinator::result_(ConfigBootCode code,
    ConfigProvisionCode provisionCode) const {
  return ConfigBootResult{code, state_, provisionCode};
}

void ConfigBootCoordinator::latchFault_() {
  state_ = CFG_BOOT_FAULT;
  selected_ = CfgPick{CB_FAULT, -1, 0};
}

ConfigBootResult ConfigBootCoordinator::start() {
  if (state_ != CFG_BOOT_COLD) return result_(CFG_BOOT_ALREADY_STARTED);

  // Latch fault first. Every successful branch below must explicitly establish
  // READY or NEEDS_SETUP; early exits can never leave a partly initialized state.
  latchFault_();
  if (!backend_.begin()) return result_(CFG_BOOT_BACKEND_BEGIN_FAILED);

  Config candidate;
  CfgPick picked = {CB_FAULT, -1, 0};
  if (!backend_.load(candidate, picked)) return result_(CFG_BOOT_LOAD_FAILED);

  if (picked.boot == CB_UNPROVISIONED && picked.active == -1 && picked.seq == 0) {
    selected_ = picked;
    state_ = CFG_BOOT_NEEDS_SETUP;
    return result_(CFG_BOOT_SETUP_REQUIRED);
  }

  if (picked.boot != CB_OK || (picked.active != 0 && picked.active != 1) ||
      picked.seq == 0 || !validateConfig(candidate)) {
    return result_(CFG_BOOT_INVALID_SELECTION);
  }

  config_ = candidate;
  selected_ = picked;
  state_ = CFG_BOOT_READY;
  return result_(CFG_BOOT_OK);
}

ConfigBootResult ConfigBootCoordinator::provision(const Config& initial,
    const IConfigProvisionGate& gate) {
  if (state_ != CFG_BOOT_NEEDS_SETUP)
    return result_(CFG_BOOT_NOT_SETUP_REQUIRED);

  ConfigProvisioner provisioner(backend_, gate);
  const ConfigProvisionResult written = provisioner.provision(initial, selected_);
  if (written.code == CFG_PROVISION_NOT_AUTHORIZED ||
      written.code == CFG_PROVISION_INVALID_CONFIG) {
    // These failures happen before any storage mutation and can be corrected by
    // the setup caller without rebooting.
    return result_(CFG_BOOT_PROVISION_REJECTED, written.code);
  }
  if (written.code != CFG_PROVISION_OK) {
    latchFault_();
    return result_(CFG_BOOT_PROVISION_FAILED, written.code);
  }

  // Do not trust the provisioner's returned selection as the boot state. Re-open
  // through the backend's canonical load path and compare the durable selection.
  Config reloaded;
  CfgPick picked = {CB_FAULT, -1, 0};
  if (!backend_.load(reloaded, picked)) {
    latchFault_();
    return result_(CFG_BOOT_RELOAD_FAILED, written.code);
  }
  if (picked.boot != CB_OK || picked.active != written.selected.active ||
      picked.seq != written.selected.seq || !validateConfig(reloaded) ||
      !configEqual(reloaded, initial)) {
    latchFault_();
    return result_(CFG_BOOT_RELOAD_FAILED, written.code);
  }

  config_ = reloaded;
  selected_ = picked;
  state_ = CFG_BOOT_READY;
  return result_(CFG_BOOT_OK, written.code);
}

bool ConfigBootCoordinator::selection(CfgPick& out) const {
  if (!ready()) return false;
  out = selected_;
  return true;
}

}  // namespace att
