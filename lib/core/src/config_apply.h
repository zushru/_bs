#pragma once
#include "api_logic.h"

namespace att {

// Backend contract: writeSlot() returns true only after its durable commit/flush.
// The coordinator still reads the inactive slot back and decodes it before applying.
// A false write result is treated as indeterminate: the backend may have persisted
// some or all bytes, so this coordinator latches fault and refuses further updates.
class IConfigSlotBackend {
 public:
  virtual ~IConfigSlotBackend() {}
  virtual bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) = 0;
  virtual bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) = 0;
};

// Side-effect-free preflight followed by an apply that must be serialized with all
// engine/pipeline work. If applyAfterCommit() fails, persistence already contains the
// new generation: caller MUST enter CONFIG_FAULT and stop normal processing.
class IConfigApplyRuntime {
 public:
  virtual ~IConfigApplyRuntime() {}
  virtual bool canApply(const Config& next) const = 0;
  virtual bool applyAfterCommit(const Config& next) = 0;
};

// Read both A/B slots. False means I/O failure or corrupt non-blank slots (CB_FAULT).
// On CB_OK, `out` and `pick` are updated. On CB_UNPROVISIONED, only `pick` is updated.
bool readConfigSlots(IConfigSlotBackend& backend, Config& out, CfgPick& pick);

enum ConfigApplyCode : uint8_t {
  CFG_APPLY_OK,
  CFG_APPLY_UNCHANGED,
  CFG_APPLY_NOT_READY,
  CFG_APPLY_REJECTED,
  CFG_APPLY_BUSY,
  CFG_APPLY_SEQUENCE_EXHAUSTED,
  CFG_APPLY_STORAGE_INDETERMINATE,
  CFG_APPLY_VERIFY_FAILED,
  CFG_APPLY_RUNTIME_FAILED,
  CFG_APPLY_FAULTED
};

struct ConfigApplyResult {
  ConfigApplyCode code;
  ApiResult gate;             // meaningful only for CFG_APPLY_REJECTED/BUSY
  uint32_t generation;        // current generation, or the newly committed one
};

// Main-loop-only A/B configuration transaction coordinator. Construct only after
// readConfigSlots() returned CB_OK and after live runtime objects were built from that
// exact selected Config. Do not share it across tasks without an external mutex.
class ConfigApplyCoordinator {
 public:
  ConfigApplyCoordinator(IConfigSlotBackend& backend, IConfigApplyRuntime& runtime,
                         const Config& current, const CfgPick& selected);
  ConfigApplyCoordinator(const ConfigApplyCoordinator&) = delete;
  ConfigApplyCoordinator& operator=(const ConfigApplyCoordinator&) = delete;

  ConfigApplyResult apply(const Config& next, const ConfigGateIn& gate);
  bool ready() const { return ready_ && !faulted_; }
  bool faulted() const { return faulted_; }
  uint32_t generation() const { return seq_; }
  int activeSlot() const { return active_; }
  const Config& current() const { return current_; }

 private:
  ConfigApplyResult result_(ConfigApplyCode code, ApiCode http = A_OK,
                            ApiReason reason = R_NONE) const;
  IConfigSlotBackend& backend_;
  IConfigApplyRuntime& runtime_;
  Config current_;
  uint32_t seq_;
  int active_;
  bool ready_;
  bool faulted_;
};

}  // namespace att
