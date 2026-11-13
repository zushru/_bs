#pragma once
#include <string.h>
#include <vector>
#include "istorage.h"
#include "closure_engine.h"

struct MemStorage : att::IStorage {
  std::vector<uint8_t> d;
  bool failAppend = false, failTruncate = false;
  int tearBytes = -1;                        // >=0: next append writes only this many bytes, then fails
  int failAppendAt = 0;                      // 1-based append call; fail before writing
  int appendCalls = 0;
  bool completeAppendThenFail = false;      // next append writes all bytes but reports failure
  bool sizeChecked(size_t& out) override { out = d.size(); return true; }
  bool read(size_t off, uint8_t* b, size_t n) override {
    if (off + n > d.size()) return false;
    memcpy(b, &d[off], n);
    return true;
  }
  bool append(const uint8_t* b, size_t n) override {
    ++appendCalls;
    if (failAppend || (failAppendAt > 0 && appendCalls == failAppendAt)) return false;
    if (tearBytes >= 0) { d.insert(d.end(), b, b + tearBytes); tearBytes = -1; return false; }
    d.insert(d.end(), b, b + n);
    if (completeAppendThenFail) { completeAppendThenFail = false; return false; }
    return true;
  }
  bool truncate(size_t n) override {
    if (failTruncate) return false;
    d.resize(n);
    return true;
  }
};
struct FakeRoster : att::IRoster {
  uint16_t n;
  explicit FakeRoster(uint16_t k) : n(k) {}
  bool expected(uint16_t id, int32_t) override { return id >= 1 && id <= n; }
  bool historyComplete() const override { return true; }
};
