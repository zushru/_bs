#pragma once
#include <stddef.h>
#include <stdint.h>
namespace att {
// One logical append-only byte stream of 16-byte records. Month files, quarantine and
// truncate-by-rewrite live in the driver, not here.
class IStorage {
 public:
  virtual ~IStorage() {}
  virtual size_t size() = 0;
  virtual bool read(size_t off, uint8_t* buf, size_t n) = 0;
  virtual bool append(const uint8_t* buf, size_t n) = 0;   // write + flush + verify; false = not durable
  virtual bool truncate(size_t n) = 0;
};
}
