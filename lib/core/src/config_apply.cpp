#include "config_apply.h"

namespace att {

bool readConfigSlots(IConfigSlotBackend& backend, Config& out, CfgPick& pick) {
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  if (!backend.readSlot(0, a) || !backend.readSlot(1, b)) {
    pick = CfgPick{CB_FAULT, -1, 0};
    return false;
  }
  Config selected;
  CfgPick p = pickSlot(a, b, selected);
  pick = p;
  if (p.boot == CB_OK) out = selected;
  return p.boot != CB_FAULT;
}

ConfigApplyCoordinator::ConfigApplyCoordinator(IConfigSlotBackend& backend,
    IConfigApplyRuntime& runtime, const Config& current, const CfgPick& selected)
    : backend_(backend), runtime_(runtime), current_(current), seq_(selected.seq),
      active_(selected.active),
      ready_(selected.boot == CB_OK && (selected.active == 0 || selected.active == 1) &&
             validateConfig(current)), faulted_(false) {}

ConfigApplyResult ConfigApplyCoordinator::result_(ConfigApplyCode code,
    ApiCode http, ApiReason reason) const {
  return ConfigApplyResult{code, ApiResult{http, reason, 0}, seq_};
}

ConfigApplyResult ConfigApplyCoordinator::apply(const Config& next,
                                                 const ConfigGateIn& suppliedGate) {
  if (faulted_) return result_(CFG_APPLY_FAULTED, A_LOCKED, R_STATE);
  if (!ready_) return result_(CFG_APPLY_NOT_READY, A_LOCKED, R_STATE);

  // Never trust a caller-supplied current generation. The coordinator owns it.
  ConfigGateIn gate = suppliedGate;
  gate.curGen = seq_;
  ApiResult allowed = gateConfigPut(current_, next, gate);
  if (allowed.code != A_OK)
    return ConfigApplyResult{CFG_APPLY_REJECTED, allowed, seq_};

  if (configEqual(current_, next))
    return result_(CFG_APPLY_UNCHANGED);
  if (!runtime_.canApply(next))
    return result_(CFG_APPLY_BUSY, A_CONFLICT, R_STATE);
  if (seq_ == UINT32_MAX)
    return result_(CFG_APPLY_SEQUENCE_EXHAUSTED, A_LOCKED, R_STATE);

  const uint32_t nextSeq = seq_ + 1;
  const uint8_t target = (uint8_t)(1 - active_);
  uint8_t encoded[CFG_SLOT_SIZE];
  if (encodeConfig(next, nextSeq, encoded) == 0) {
    // gateConfigPut should already have caught invalid data; treat disagreement as a
    // coordinator/runtime contract failure, not as a storage write.
    return result_(CFG_APPLY_REJECTED, A_INVALID, R_BAD_FIELD);
  }

  // A write error may occur after the backend has committed. Do not guess which slot
  // is active: latch the coordinator until reboot reloads and selects the A/B pair.
  if (!backend_.writeSlot(target, encoded)) {
    faulted_ = true;
    return result_(CFG_APPLY_STORAGE_INDETERMINATE, A_LOCKED, R_STATE);
  }

  uint8_t check[CFG_SLOT_SIZE];
  Config decoded;
  uint32_t decodedSeq = 0;
  if (!backend_.readSlot(target, check) ||
      decodeConfig(check, decoded, decodedSeq) != CD_OK ||
      decodedSeq != nextSeq || !configEqual(decoded, next)) {
    faulted_ = true;
    return result_(CFG_APPLY_VERIFY_FAILED, A_LOCKED, R_STATE);
  }

  // Runtime changes happen only after durable write + read-back verification. If this
  // unexpectedly fails, persistent state is newer than RAM; fail closed until reboot.
  if (!runtime_.applyAfterCommit(next)) {
    faulted_ = true;
    return result_(CFG_APPLY_RUNTIME_FAILED, A_LOCKED, R_STATE);
  }

  current_ = next;
  seq_ = nextSeq;
  active_ = target;
  return result_(CFG_APPLY_OK);
}

}  // namespace att
