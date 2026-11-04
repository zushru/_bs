#include "month_set.h"
namespace att {
size_t MonthSet::size() {
  size_t t = 0;
  for (size_t i = 0; i < f_.count(); i++) t += f_.fileSize(i);
  return t;
}
bool MonthSet::read(size_t off, uint8_t* buf, size_t n) {
  size_t start = 0;
  for (size_t i = 0; i < f_.count() && n > 0; i++) {
    size_t sz = f_.fileSize(i);
    if (off < start + sz) {
      size_t local = off - start, take = sz - local;
      if (take > n) take = n;
      if (!f_.readFile(i, local, buf, take)) return false;
      buf += take; off += take; n -= take;
    }
    start += sz;
  }
  return n == 0;
}
bool MonthSet::append(const uint8_t* buf, size_t n) {
  size_t c = f_.count();
  if (month_ == 0) return false;                      // fail-closed: caller never set the month (e.g. after boot)
  if (c == 0 || (month_ > f_.monthKey(c - 1))) {
    if (c > 0 && (f_.fileSize(c - 1) % 16) != 0) return false;   // would misalign the stream
    if (!f_.addFile(month_)) return false;
    c = f_.count();
  }
  return f_.appendFile(c - 1, buf, n);                 // same or earlier month: newest file
}
bool MonthSet::truncate(size_t n) {
  refused_ = (size_t)-1;                              // only the most recent refusal is reported
  size_t start = 0, c = f_.count();
  for (size_t i = 0; i < c; i++) {
    size_t sz = f_.fileSize(i);
    if (n <= start + sz) {
      for (size_t j = i + 1; j < c; j++)
        if (f_.fileSize(j) != 0) { refused_ = n; return false; }
      return f_.truncateFile(i, n - start);
    }
    start += sz;
  }
  return false;
}
QuarantinePlan MonthSet::planAt(size_t off) {
  QuarantinePlan p = {false, 0, 0};
  size_t start = 0;
  for (size_t i = 0; i < f_.count(); i++) {
    size_t sz = f_.fileSize(i);
    if (off < start + sz) { p.valid = true; p.fileIndex = i; p.localOff = off - start; return p; }
    start += sz;
  }
  return p;
}
bool MonthSet::repair(const QuarantinePlan& p) {
  if (!p.valid || p.fileIndex >= f_.count()) return false;
  if (!f_.quarantineCopy(p.fileIndex)) return false;
  if (f_.count() > p.fileIndex + 1 && !f_.quarantineFrom(p.fileIndex + 1)) return false;
  if (!f_.truncateFile(p.fileIndex, p.localOff)) return false;
  refused_ = (size_t)-1;
  return true;
}
}
