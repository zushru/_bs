#pragma once
#include <stddef.h>
#include <stdint.h>

class TwoWire {
 public:
  TwoWire() : address(0), pointer(0), txLength(0), rxIndex(0), rxLength(0),
      endResult(0), requestLimit(static_cast<size_t>(-1)), writeLimit(static_cast<size_t>(-1)),
      busBeginCalls(0), busBeginResult(true), busSda(-1), busScl(-1), busClockHz(0),
      beginCalls(0), endCalls(0), requestCalls(0), requestedLength(0),
      lastEndStop(true), lastRequestStop(true), lockHeldProbe(nullptr),
      unlockedAccessCalls(0) {
    for (size_t i = 0; i < sizeof registers; ++i) registers[i] = 0;
    for (size_t i = 0; i < sizeof tx; ++i) tx[i] = 0;
    for (size_t i = 0; i < sizeof rx; ++i) rx[i] = 0;
  }

  bool begin(int sda, int scl, uint32_t frequency) {
    ++busBeginCalls;
    busSda = sda;
    busScl = scl;
    busClockHz = frequency;
    return busBeginResult;
  }

  void beginTransmission(uint8_t addr) {
    if (lockHeldProbe && !*lockHeldProbe) ++unlockedAccessCalls;

    address = addr;
    txLength = 0;
    ++beginCalls;
  }

  size_t write(uint8_t value) {
    if (lockHeldProbe && !*lockHeldProbe) ++unlockedAccessCalls;

    if (txLength >= sizeof tx || txLength >= writeLimit) return 0;
    tx[txLength++] = value;
    return 1;
  }

  uint8_t endTransmission(bool stopBit) {
    if (lockHeldProbe && !*lockHeldProbe) ++unlockedAccessCalls;

    ++endCalls;
    lastEndStop = stopBit;
    if (endResult != 0) return endResult;
    if (txLength == 0) return 4;
    pointer = tx[0];
    if (txLength > 1) {
      for (size_t i = 1; i < txLength; ++i) {
        if ((size_t)pointer + i - 1 < sizeof registers)
          registers[pointer + i - 1] = tx[i];
      }
    }
    return 0;
  }

  size_t requestFrom(uint8_t addr, size_t length, bool stopBit) {
    if (lockHeldProbe && !*lockHeldProbe) ++unlockedAccessCalls;

    ++requestCalls;
    requestedAddress = addr;
    requestedLength = length;
    lastRequestStop = stopBit;
    rxIndex = 0;
    rxLength = 0;
    size_t wanted = length < requestLimit ? length : requestLimit;
    for (size_t i = 0; i < wanted && (size_t)pointer + i < sizeof registers &&
                        i < sizeof rx; ++i) {
      rx[i] = registers[pointer + i];
      ++rxLength;
    }
    pointer = static_cast<uint8_t>(pointer + rxLength);
    return rxLength;
  }

  int available() const {
    if (lockHeldProbe && !*lockHeldProbe) ++unlockedAccessCalls;
    return static_cast<int>(rxLength - rxIndex);
  }
  int read() {
    if (lockHeldProbe && !*lockHeldProbe) ++unlockedAccessCalls;

    if (rxIndex >= rxLength) return -1;
    return rx[rxIndex++];
  }

  uint8_t address;
  uint8_t requestedAddress;
  uint8_t pointer;
  uint8_t registers[19];
  uint8_t tx[16];
  uint8_t rx[16];
  size_t txLength;
  size_t rxIndex;
  size_t rxLength;
  uint8_t endResult;
  size_t requestLimit;
  size_t writeLimit;
  unsigned busBeginCalls;
  bool busBeginResult;
  int busSda;
  int busScl;
  uint32_t busClockHz;
  unsigned beginCalls;
  unsigned endCalls;
  unsigned requestCalls;
  size_t requestedLength;
  bool lastEndStop;
  bool lastRequestStop;
  bool* lockHeldProbe;
  mutable unsigned unlockedAccessCalls;
};
