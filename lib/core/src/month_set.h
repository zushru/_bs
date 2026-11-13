#pragma once
#include "istorage.h"
#include "fileset.h"
namespace att {
struct QuarantinePlan { bool valid; size_t fileIndex; size_t localOff; };
// One logical append-only stream over month files. Commit groups never straddle files.
class MonthSet : public IStorage {
 public:
  explicit MonthSet(IFileSet& f) : f_(f), month_(0), refused_((size_t)-1) {}
  void setMonth(uint32_t key) { month_ = key; }     // caller sets from the RTC before appending
  bool usesFileSet(const IFileSet& files) const { return &f_ == &files; }
  bool sizeChecked(size_t& out) override;
  bool read(size_t off, uint8_t* buf, size_t n) override;
  bool append(const uint8_t* buf, size_t n) override;
  bool truncate(size_t n) override;                 // only within the last non-empty file
  size_t refusedTruncate() const { return refused_; }   // (size_t)-1 = none
  QuarantinePlan planAt(size_t byteOff);            // corruptAt, or refusedTruncate()
  bool repair(const QuarantinePlan& p);             // copy, move later files, then truncate
 private:
  IFileSet& f_;
  uint32_t month_;
  size_t refused_;
};
}
