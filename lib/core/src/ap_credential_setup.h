#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ap_credential_display.h"
#include "config_boot.h"
#include "credential_soft_ap.h"

namespace att {

// Product-fixed first-provision network identity. Keep generic; do not encode
// a school, owner, MAC address, or other device identifier in the SSID.
static const char kProductionSoftApSsid[] = "att";
static const int kProductionSoftApChannel = 1;

enum ApCredentialSetupState : uint8_t {
  ACSS_COLD,
  ACSS_WAITING_FOR_CLIENT,
  ACSS_CLEAR_PENDING,          // Config is READY; OLED blank request is pending.
  ACSS_STARTUP_CLEAR_PENDING,  // Faulted setup; only an OLED blank retry is allowed.
  ACSS_PROVISIONED,
  ACSS_FAULT
};

enum ApCredentialSetupCode : uint8_t {
  ACSE_STARTED,
  ACSE_ALREADY_STARTED,
  ACSE_BOOT_NOT_SETUP_REQUIRED,
  ACSE_BOOTSTRAP_FAILED,
  ACSE_DISPLAY_FAILED,
  ACSE_AP_START_FAILED,
  ACSE_NOT_READY_FOR_PROVISION,
  ACSE_CONFIG_REJECTED,
  ACSE_PROVISION_FAULTED,
  ACSE_PROVISIONED_CLEAR_REQUESTED,
  ACSE_PROVISIONED_CLEAR_PENDING,
  ACSE_STARTUP_CLEAR_REQUESTED,
  ACSE_STARTUP_CLEAR_PENDING
};

// First-setup coordinator. It owns the strict order:
//   NEEDS_SETUP -> durable credential bootstrap -> OLED transfer request
//   -> secured AP start -> client DHCP assignment -> setup-auth gate -> config
//   provisioning -> blank-frame request.
//
// The production SSID and channel are fixed by product policy. The coordinator
// has no credential rotation/reset API; see E30 in DECISIONS.md. Calls are
// serialized on the application task.
class ApCredentialSetupCoordinator {
 public:
  ApCredentialSetupCoordinator(ConfigBootCoordinator& boot,
                               ApCredentialBootstrap& bootstrap,
                               IApCredentialDisplay& display,
                               ICredentialSoftAp& ap);
  ApCredentialSetupCoordinator(const ApCredentialSetupCoordinator&) = delete;
  ApCredentialSetupCoordinator& operator=(const ApCredentialSetupCoordinator&) = delete;

  // Starts only from CFG_BOOT_NEEDS_SETUP. The passphrase is never returned to
  // the caller and its temporary buffer is cleared on every exit path.
  ApCredentialSetupCode start();

  // Supplies a per-request authorization gate to ConfigBootCoordinator through
  // a short-lived composite gate. A successful durable config provision is
  // followed by a blank-frame request. The AP stays running (signed policy).
  // bootResult is updated only when ConfigBootCoordinator::provision() is
  // reached; earlier gate/bootstrap failures leave the caller's value intact.
  ApCredentialSetupCode provision(const Config& initial,
                                  const IConfigProvisionGate& requestAuthGate,
                                  ConfigBootResult& bootResult);

  // Retries only a blank-frame request after a startup/provision fault or a
  // successful durable provision. This never restarts AP or changes config.
  ApCredentialSetupCode retryBlankFrame();

  // Authorization is supplied for the current synchronous request. The gate
  // must be side-effect-free/idempotent because it can be checked both here
  // and by ConfigProvisioner; it is never retained or cached between calls.
  bool mayProvision(const IConfigProvisionGate& requestAuthGate) const;
  ApCredentialSetupState state() const { return state_; }

 private:
  static void clearSecret_(void* value, size_t length);
  static bool validDisclosureSecret_(const char* value, size_t length);
  bool requestFaultClear_();

  ConfigBootCoordinator& boot_;
  ApCredentialBootstrap& bootstrap_;
  IApCredentialDisplay& display_;
  ICredentialSoftAp& ap_;
  ApCredentialSetupState state_;
};

}  // namespace att
