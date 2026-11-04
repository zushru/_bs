#pragma once
#include <stdint.h>
namespace att {
enum Derived : uint8_t { D_NA = 0, D_NOCLASS, D_UPCOMING, D_PENDING, D_PRESENT, D_LATE, D_EXCUSED, D_ABSENT, D_UNVERIFIED };
struct Facts {
  bool noClass;        // session or whole-day no-class in force
  bool expected;       // live roster if not closed; CLOSE snapshot if closed
  bool started;        // now >= session start
  bool closed;         // CLOSE record exists for (day, session)
  bool coverage;       // coverage flag from that CLOSE
  uint8_t markStatus;  // latest MARK: 0 none/CLEAR, 1 P, 2 L, 3 E
  uint8_t scanStatus;  // first SCAN, status frozen at write time: 0 none, 1 P, 2 L
};
Derived derive(const Facts& f);
}
