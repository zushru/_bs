#include "status_derive.h"
namespace att {
Derived derive(const Facts& f) {
  if (f.noClass) return D_NOCLASS;
  if (f.markStatus == 1) return D_PRESENT;
  if (f.markStatus == 2) return D_LATE;
  if (f.markStatus == 3) return D_EXCUSED;
  if (f.scanStatus == 1) return D_PRESENT;
  if (f.scanStatus == 2) return D_LATE;
  if (!f.expected) return D_NA;
  if (!f.started) return D_UPCOMING;
  if (!f.closed) return D_PENDING;
  return f.coverage ? D_ABSENT : D_UNVERIFIED;
}
}
