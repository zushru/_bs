#include "month_set.h"
namespace att {
bool MonthSet::sizeChecked(size_t& out) {
  out = 0;
  size_t count = 0;
  if (!f_.countChecked(count)) return false;
  for (size_t i = 0; i < count; ++i) {
    size_t bytes = 0;
    if (!f_.fileSizeChecked(i, bytes) || bytes > (size_t)-1 - out)
      return false;
    out += bytes;
  }
  return true;
}
bool MonthSet::read(size_t off, uint8_t* buf, size_t n) {
  if (n == 0) return true;
  if (!buf) return false;
  size_t count = 0;
  if (!f_.countChecked(count)) return false;
  size_t start = 0;
  for (size_t i = 0; i < count && n > 0; ++i) {
    size_t bytes = 0;
    if (!f_.fileSizeChecked(i, bytes)) return false;
    if (off >= start && off - start < bytes) {
      const size_t local = off - start;
      size_t take = bytes - local;
      if (take > n) take = n;
      if (!f_.readFile(i, local, buf, take)) return false;
      buf += take;
      off += take;
      n -= take;
    }
    if (bytes > (size_t)-1 - start) return false;
    start += bytes;
  }
  return n == 0;
}
bool MonthSet::append(const uint8_t* buf, size_t n) {
  if (month_ == 0 || (!buf && n) || n == 0) return false;
  size_t count = 0;
  if (!f_.countChecked(count)) return false;
  bool create = count == 0;
  if (count > 0) {
    uint32_t newestKey = 0;
    size_t newestSize = 0;
    if (!f_.monthKeyChecked(count - 1, newestKey) ||
        !f_.fileSizeChecked(count - 1, newestSize)) return false;
    create = month_ > newestKey;
    if (create && (newestSize % 16) != 0) return false;
  }
  if (create) {
    if (count == (size_t)-1 || !f_.addFile(month_)) return false;
    size_t after = 0, newSize = 0;
    uint32_t newKey = 0;
    if (!f_.countChecked(after) || after != count + 1 || after == 0 ||
        !f_.monthKeyChecked(after - 1, newKey) || newKey != month_ ||
        !f_.fileSizeChecked(after - 1, newSize) || newSize != 0)
      return false;
    count = after;
  }
  return count > 0 && f_.appendFile(count - 1, buf, n);
}
bool MonthSet::truncate(size_t n) {
  refused_ = (size_t)-1;
  size_t count = 0;
  if (!f_.countChecked(count)) return false;
  size_t start = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t bytes = 0;
    if (!f_.fileSizeChecked(i, bytes) || bytes > (size_t)-1 - start)
      return false;
    if (n >= start && n - start <= bytes) {
      for (size_t j = i + 1; j < count; ++j) {
        size_t later = 0;
        if (!f_.fileSizeChecked(j, later)) return false;
        if (later != 0) { refused_ = n; return false; }
      }
      return f_.truncateFile(i, n - start);
    }
    start += bytes;
  }
  return false;
}
QuarantinePlan MonthSet::planAt(size_t off) {
  QuarantinePlan p = {false, 0, 0};
  size_t count = 0;
  if (!f_.countChecked(count)) return p;
  size_t start = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t bytes = 0;
    if (!f_.fileSizeChecked(i, bytes) || bytes > (size_t)-1 - start)
      return p;
    if (off >= start && off - start < bytes) {
      p.valid = true;
      p.fileIndex = i;
      p.localOff = off - start;
      return p;
    }
    start += bytes;
  }
  return p;
}
bool MonthSet::repair(const QuarantinePlan& p) {
  size_t count = 0;
  if (!p.valid || !f_.countChecked(count) || p.fileIndex >= count)
    return false;
  if (!f_.quarantineCopy(p.fileIndex)) return false;
  if (count > p.fileIndex + 1 && !f_.quarantineFrom(p.fileIndex + 1)) return false;
  if (!f_.truncateFile(p.fileIndex, p.localOff)) return false;
  refused_ = (size_t)-1;
  return true;
}
}
