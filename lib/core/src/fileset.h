#pragma once
#include <stddef.h>
#include <stdint.h>
namespace att {
// Ordered set of month files (oldest first). The driver implements this over LittleFS.
class IFileSet {
 public:
  virtual ~IFileSet() {}
  virtual size_t count() = 0;
  virtual uint32_t monthKey(size_t i) = 0;                    // e.g. 202611; strictly increasing
  virtual size_t fileSize(size_t i) = 0;
  virtual bool readFile(size_t i, size_t off, uint8_t* buf, size_t n) = 0;
  virtual bool appendFile(size_t i, const uint8_t* buf, size_t n) = 0;   // durable or false
  virtual bool truncateFile(size_t i, size_t n) = 0;
  virtual bool addFile(uint32_t monthKey) = 0;                // new empty file at the end
  virtual bool quarantineCopy(size_t i) = 0;                  // copy file i aside, keep original
  virtual bool quarantineFrom(size_t i) = 0;                  // move files i..end aside
};
}
