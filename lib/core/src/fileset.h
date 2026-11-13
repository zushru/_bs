#pragma once
#include <stddef.h>
#include <stdint.h>
namespace att {
// Ordered set of month files (oldest first). The driver implements this over LittleFS.
class IFileSet {
 public:
  virtual ~IFileSet() {}
  // Metadata operations must expose filesystem failures explicitly. Do not replace
  // these with value-only wrappers: zero is valid metadata and cannot mean I/O error.
  virtual bool countChecked(size_t& out) = 0;
  virtual bool monthKeyChecked(size_t i, uint32_t& out) = 0;  // e.g. 202611; strictly increasing
  virtual bool fileSizeChecked(size_t i, size_t& out) = 0;
  virtual bool readFile(size_t i, size_t off, uint8_t* buf, size_t n) = 0;
  // true = complete bytes passed the driver's durability criterion.
  // false = indeterminate: the file may have been partially or fully modified.
  virtual bool appendFile(size_t i, const uint8_t* buf, size_t n) = 0;
  virtual bool truncateFile(size_t i, size_t n) = 0;
  virtual bool addFile(uint32_t monthKey) = 0;                // new empty file at the end
  virtual bool quarantineCopy(size_t i) = 0;                  // copy file i aside, keep original
  virtual bool quarantineFrom(size_t i) = 0;                  // move files i..end aside
};
}
