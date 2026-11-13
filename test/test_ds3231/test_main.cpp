#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "ds3231_clock.h"
#include "esp32_ds3231_register_io.h"
#include "i2c_bus_lock.h"
#include "Wire.h"

using namespace att;

namespace {
class TestI2cBusLock : public II2cBusLock {
 public:
  TestI2cBusLock() : allow(true), held(false), acquireCalls(0), releaseCalls(0) {}
  bool acquire() override {
    ++acquireCalls;
    if (!allow || held) return false;
    held = true;
    return true;
  }
  void release() override {
    ++releaseCalls;
    held = false;
  }
  bool allow;
  bool held;
  unsigned acquireCalls;
  unsigned releaseCalls;
};

const uint8_t kTimeReg = 0x00;
const uint8_t kStatusReg = 0x0F;

uint8_t bcd(uint8_t v) { return static_cast<uint8_t>((v / 10u) << 4u) | (v % 10u); }

class FakeRegisterIo : public IDs3231RegisterIo {
 public:
  FakeRegisterIo() : readFails(false), writeFails(false), failReadRegisterEnabled(false),
      failReadRegister(0), failReadCall(0), injectAlarmFlagsBeforeNextStatusWrite(false),
      setOsfBeforeSecondStatusRead(false), readCalls(0), writeCalls(0),
      lastReadReg(0), lastWriteReg(0), lastWriteLength(0) {
    memset(regs, 0, sizeof regs);
    memset(lastWrite, 0, sizeof lastWrite);
  }
  bool readRegisters(uint8_t first, uint8_t* out, uint8_t length) override {
    ++readCalls;
    lastReadReg = first;
    if (first == kStatusReg && readCalls == 2 && setOsfBeforeSecondStatusRead)
      regs[kStatusReg] = static_cast<uint8_t>(regs[kStatusReg] | 0x80u);
    if (readFails || (failReadCall != 0 && readCalls == failReadCall) ||
        (failReadRegisterEnabled && first == failReadRegister) ||
        out == nullptr || length == 0 ||
        static_cast<unsigned>(first) + length > sizeof regs) return false;
    memcpy(out, regs + first, length);
    return true;
  }
  Ds3231WriteResult writeRegisters(uint8_t first, const uint8_t* data,
                                    uint8_t length) override {
    ++writeCalls;
    lastWriteReg = first;
    lastWriteLength = length;
    if (writeFails || data == nullptr || length == 0 ||
        static_cast<unsigned>(first) + length > sizeof regs)
      return DS3231_WRITE_UNCERTAIN;
    memcpy(lastWrite, data, length);
    if (first == kStatusReg && length == 1) {
      if (injectAlarmFlagsBeforeNextStatusWrite) {
        regs[kStatusReg] = static_cast<uint8_t>(regs[kStatusReg] | 0x03u);
        injectAlarmFlagsBeforeNextStatusWrite = false;
      }
      // A1F/A2F are write-zero-to-clear; writing one preserves their live
      // hardware state, including a flag latched after the preceding read.
      const uint8_t alarmMask = 0x03u;
      const uint8_t current = regs[kStatusReg];
      regs[kStatusReg] = static_cast<uint8_t>(
          (data[0] & static_cast<uint8_t>(~alarmMask)) |
          (current & data[0] & alarmMask));
    } else {
      memcpy(regs + first, data, length);
    }
    return DS3231_WRITE_OK;
  }
  void set24(uint8_t sec, uint8_t min, uint8_t hour, uint8_t dow,
             uint8_t day, uint8_t month, uint8_t year, bool century = false) {
    regs[0] = bcd(sec);
    regs[1] = bcd(min);
    regs[2] = bcd(hour);
    regs[3] = bcd(dow);
    regs[4] = bcd(day);
    regs[5] = static_cast<uint8_t>(bcd(month) | (century ? 0x80u : 0u));
    regs[6] = bcd(year);
  }
  bool readFails;
  bool writeFails;
  bool failReadRegisterEnabled;
  uint8_t failReadRegister;
  unsigned failReadCall;
  bool injectAlarmFlagsBeforeNextStatusWrite;
  bool setOsfBeforeSecondStatusRead;
  unsigned readCalls;
  unsigned writeCalls;
  uint8_t lastReadReg;
  uint8_t lastWriteReg;
  uint8_t lastWriteLength;
  uint8_t regs[19];
  uint8_t lastWrite[8];
};
}  // namespace

void setUp() {}
void tearDown() {}

void test_read_converts_local_pht_wall_time_to_utc_epoch() {
  FakeRegisterIo io;
  io.set24(56, 34, 12, 4, 11, 11, 26);
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(480, out));
  TEST_ASSERT_EQUAL_UINT32(1794371696u, out.epochUtc);
  TEST_ASSERT_FALSE(out.oscillatorStopped);
  TEST_ASSERT_EQUAL_UINT32(2, io.readCalls);
  TEST_ASSERT_EQUAL_UINT8(kStatusReg, io.lastReadReg);
}

void test_read_handles_positive_offset_crossing_previous_utc_day() {
  FakeRegisterIo io;
  io.set24(0, 30, 0, 4, 11, 11, 26);
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(480, out));
  TEST_ASSERT_EQUAL_UINT32(1794328200u, out.epochUtc);
}

void test_read_handles_negative_offset_crossing_next_utc_day() {
  FakeRegisterIo io;
  io.set24(0, 0, 20, 4, 11, 11, 26);
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(-300, out));
  TEST_ASSERT_EQUAL_UINT32(1794445200u, out.epochUtc);
}

void test_read_supports_twelve_hour_midnight_and_noon() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  io.regs[0] = bcd(0); io.regs[1] = bcd(0); io.regs[2] = 0x52; // 12 AM
  io.regs[3] = 7; io.regs[4] = bcd(1); io.regs[5] = bcd(1); io.regs[6] = bcd(26);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(1767225600u, out.epochUtc);
  io.regs[2] = 0x72; // 12 PM
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(1767268800u, out.epochUtc);
}

void test_read_accepts_gregorian_leap_day() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 5, 29, 2, 24);
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(1709164800u, out.epochUtc);
}

void test_read_rejects_february_29_in_non_leap_year() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 7, 29, 2, 25);
  Ds3231Clock clock(io);
  Ds3231Reading out = {123, true};
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(123u, out.epochUtc);
  TEST_ASSERT_TRUE(out.oscillatorStopped);
}

void test_read_supports_century_bit_for_year_2100() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 6, 1, 1, 0, true);
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(4102444800u, out.epochUtc);
}

void test_read_rejects_february_29_in_2100() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 1, 29, 2, 0, true);
  Ds3231Clock clock(io);
  Ds3231Reading out = {123, true};
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(123u, out.epochUtc);
  TEST_ASSERT_TRUE(out.oscillatorStopped);
}

void test_read_rejects_time_beyond_project_uint32_epoch() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 1, 8, 2, 6, true);
  Ds3231Clock clock(io);
  Ds3231Reading out = {44, false};
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
  TEST_ASSERT_EQUAL_UINT32(44u, out.epochUtc);
}

void test_read_rejects_invalid_bcd_and_reserved_bits() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 1, 1, 1, 26);
  io.regs[1] = 0x6A;
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
  io.regs[1] = 0x80;
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
}

void test_read_rejects_bad_calendar_fields_and_hour_encoding() {
  FakeRegisterIo io;
  io.set24(0, 0, 0, 1, 31, 4, 26);
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
  io.set24(0, 0, 0, 1, 1, 1, 26);
  io.regs[2] = 0x40; // 12-hour mode with hour field zero is invalid.
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.readUtcEpoch(0, out));
}

void test_read_exposes_osf_without_treating_candidate_epoch_as_trusted() {
  FakeRegisterIo io;
  io.set24(1, 2, 3, 4, 5, 6, 26);
  io.regs[kStatusReg] = 0x8B;
  Ds3231Clock clock(io);
  Ds3231Reading out = {0, false};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.readUtcEpoch(480, out));
  TEST_ASSERT_TRUE(out.oscillatorStopped);
  TEST_ASSERT_EQUAL_UINT8(0x8B, io.regs[kStatusReg]);
}

void test_read_rejects_invalid_utc_offset_without_bus_access() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  Ds3231Reading out = {9, true};
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_ARGUMENT, clock.readUtcEpoch(841, out));
  TEST_ASSERT_EQUAL_UINT32(9u, out.epochUtc);
  TEST_ASSERT_EQUAL_UINT32(0, io.readCalls);
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_ARGUMENT, clock.readUtcEpoch(-721, out));
}

void test_read_io_failure_preserves_output() {
  FakeRegisterIo io;
  io.readFails = true;
  Ds3231Clock clock(io);
  Ds3231Reading out = {77, true};
  TEST_ASSERT_EQUAL_INT(DS3231_IO_ERROR, clock.readUtcEpoch(480, out));
  TEST_ASSERT_EQUAL_UINT32(77u, out.epochUtc);
  TEST_ASSERT_TRUE(out.oscillatorStopped);
}

void test_write_converts_utc_to_configured_local_time_and_uses_24_hour_mode() {
  FakeRegisterIo io;
  io.regs[kStatusReg] = 0x8B;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.writeUtcEpoch(1794371696u, 480));
  TEST_ASSERT_EQUAL_UINT8(0, io.lastWriteReg);
  TEST_ASSERT_EQUAL_UINT8(7, io.lastWriteLength);
  TEST_ASSERT_EQUAL_UINT8(bcd(56), io.regs[0]);
  TEST_ASSERT_EQUAL_UINT8(bcd(34), io.regs[1]);
  TEST_ASSERT_EQUAL_UINT8(bcd(12), io.regs[2]);
  TEST_ASSERT_EQUAL_UINT8(bcd(4), io.regs[3]);
  TEST_ASSERT_EQUAL_UINT8(bcd(11), io.regs[4]);
  TEST_ASSERT_EQUAL_UINT8(bcd(11), io.regs[5]);
  TEST_ASSERT_EQUAL_UINT8(bcd(26), io.regs[6]);
  TEST_ASSERT_EQUAL_UINT8(0x8B, io.regs[kStatusReg]);
  TEST_ASSERT_EQUAL_UINT32(1, io.writeCalls);
}

void test_write_handles_local_midnight_rollover() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.writeUtcEpoch(1794328200u, 480));
  TEST_ASSERT_EQUAL_UINT8(bcd(0), io.regs[0]);
  TEST_ASSERT_EQUAL_UINT8(bcd(30), io.regs[1]);
  TEST_ASSERT_EQUAL_UINT8(bcd(0), io.regs[2]);
  TEST_ASSERT_EQUAL_UINT8(bcd(11), io.regs[4]);
  TEST_ASSERT_EQUAL_UINT8(bcd(11), io.regs[5]);
}

void test_write_sets_century_bit_for_year_2100() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.writeUtcEpoch(4102444800u, 0));
  TEST_ASSERT_EQUAL_UINT8(0x81, io.regs[5]);
  TEST_ASSERT_EQUAL_UINT8(0x00, io.regs[6]);
}

void test_write_rejects_pre_2000_local_time_without_mutation() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_TIME, clock.writeUtcEpoch(0, 0));
  TEST_ASSERT_EQUAL_UINT32(0, io.writeCalls);
}

void test_write_rejects_out_of_range_offset_without_mutation() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_INVALID_ARGUMENT, clock.writeUtcEpoch(1794371696u, 841));
  TEST_ASSERT_EQUAL_UINT32(0, io.writeCalls);
}

void test_write_io_failure_is_reported_as_uncertain() {
  FakeRegisterIo io;
  io.writeFails = true;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_IO_UNCERTAIN, clock.writeUtcEpoch(1794371696u, 480));
  TEST_ASSERT_EQUAL_UINT32(1, io.writeCalls);
}

void test_clear_osf_is_explicit_and_preserves_other_status_flags() {
  FakeRegisterIo io;
  io.regs[kStatusReg] = 0x8B;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.clearOscillatorStopFlag());
  TEST_ASSERT_EQUAL_UINT8(kStatusReg, io.lastWriteReg);
  TEST_ASSERT_EQUAL_UINT8(1, io.lastWriteLength);
  TEST_ASSERT_EQUAL_UINT8(0x0B, io.regs[kStatusReg]);
}

void test_clear_osf_is_noop_when_flag_is_already_clear() {
  FakeRegisterIo io;
  io.regs[kStatusReg] = 0x03;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.clearOscillatorStopFlag());
  TEST_ASSERT_EQUAL_UINT32(0, io.writeCalls);
}

void test_clear_osf_preserves_alarm_flags_latched_after_status_read() {
  FakeRegisterIo io;
  io.regs[kStatusReg] = 0x88;
  io.injectAlarmFlagsBeforeNextStatusWrite = true;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OK, clock.clearOscillatorStopFlag());
  TEST_ASSERT_EQUAL_UINT8(0x0B, io.regs[kStatusReg]);
}

void test_clear_osf_verifies_flag_remains_clear_after_write() {
  FakeRegisterIo io;
  io.regs[kStatusReg] = 0x80;
  io.setOsfBeforeSecondStatusRead = true;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_OSF_STILL_SET,
                        clock.clearOscillatorStopFlag());
  TEST_ASSERT_EQUAL_UINT32(2, io.readCalls);
}

void test_clear_osf_returns_uncertain_when_verification_read_fails() {
  FakeRegisterIo io;
  io.regs[kStatusReg] = 0x80;
  io.failReadCall = 2;
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_IO_UNCERTAIN,
                        clock.clearOscillatorStopFlag());
  TEST_ASSERT_EQUAL_UINT32(1, io.writeCalls);
}

void test_clear_osf_does_not_hide_status_read_or_write_errors() {
  FakeRegisterIo io;
  Ds3231Clock clock(io);
  io.readFails = true;
  TEST_ASSERT_EQUAL_INT(DS3231_IO_ERROR, clock.clearOscillatorStopFlag());
  io.readFails = false;
  io.regs[kStatusReg] = 0x80;
  io.writeFails = true;
  TEST_ASSERT_EQUAL_INT(DS3231_IO_UNCERTAIN, clock.clearOscillatorStopFlag());
}

void test_read_status_io_failure_preserves_output() {
  FakeRegisterIo io;
  io.set24(1, 2, 3, 4, 5, 6, 26);
  io.failReadRegisterEnabled = true;
  io.failReadRegister = kStatusReg;
  Ds3231Clock clock(io);
  Ds3231Reading out = {99, true};
  TEST_ASSERT_EQUAL_INT(DS3231_IO_ERROR, clock.readUtcEpoch(480, out));
  TEST_ASSERT_EQUAL_UINT32(99u, out.epochUtc);
  TEST_ASSERT_TRUE(out.oscillatorStopped);
}

void test_wire_register_read_uses_repeated_start_and_exact_length() {
  TwoWire wire;
  TestI2cBusLock lock;
  wire.lockHeldProbe = &lock.held;
  for (uint8_t i = 0; i < 7; ++i) wire.registers[i] = static_cast<uint8_t>(i + 10);
  Esp32Ds3231RegisterIo io(wire, lock);
  uint8_t out[7] = {};
  TEST_ASSERT_TRUE(io.readRegisters(0, out, 7));
  TEST_ASSERT_EQUAL_UINT32(1, lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(1, lock.releaseCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.unlockedAccessCalls);
  TEST_ASSERT_EQUAL_UINT8(0x68, wire.address);
  TEST_ASSERT_EQUAL_UINT8(0x68, wire.requestedAddress);
  TEST_ASSERT_EQUAL_UINT32(1, wire.beginCalls);
  TEST_ASSERT_EQUAL_UINT32(1, wire.endCalls);
  TEST_ASSERT_FALSE(wire.lastEndStop);
  TEST_ASSERT_EQUAL_UINT32(1, wire.requestCalls);
  TEST_ASSERT_EQUAL_UINT32(7, wire.requestedLength);
  TEST_ASSERT_TRUE(wire.lastRequestStop);
  for (uint8_t i = 0; i < 7; ++i) TEST_ASSERT_EQUAL_UINT8(i + 10, out[i]);
}

void test_wire_short_read_fails_without_partial_output() {
  TwoWire wire;
  TestI2cBusLock lock;
  wire.lockHeldProbe = &lock.held;
  wire.requestLimit = 3;
  Esp32Ds3231RegisterIo io(wire, lock);
  uint8_t out[7] = {1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_FALSE(io.readRegisters(0, out, 7));
  TEST_ASSERT_EQUAL_UINT32(1, lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(1, lock.releaseCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.unlockedAccessCalls);
  const uint8_t expected[7] = {1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_EQUAL_MEMORY(expected, out, sizeof out);
}

void test_wire_nack_fails_register_read() {
  TwoWire wire;
  TestI2cBusLock lock;
  wire.lockHeldProbe = &lock.held;
  wire.endResult = 2;
  Esp32Ds3231RegisterIo io(wire, lock);
  uint8_t out = 0xAA;
  TEST_ASSERT_FALSE(io.readRegisters(0x0F, &out, 1));
  TEST_ASSERT_EQUAL_UINT32(1, lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(1, lock.releaseCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.unlockedAccessCalls);
  TEST_ASSERT_EQUAL_UINT8(0xAA, out);
  TEST_ASSERT_EQUAL_UINT32(0, wire.requestCalls);
}

void test_wire_register_write_sends_one_bounded_transaction() {
  TwoWire wire;
  TestI2cBusLock lock;
  wire.lockHeldProbe = &lock.held;
  Esp32Ds3231RegisterIo io(wire, lock);
  const uint8_t data[7] = {1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_EQUAL_INT(DS3231_WRITE_OK,
                        io.writeRegisters(0, data, 7));
  TEST_ASSERT_EQUAL_UINT32(1, lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(1, lock.releaseCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.unlockedAccessCalls);
  TEST_ASSERT_EQUAL_UINT8(0x68, wire.address);
  TEST_ASSERT_TRUE(wire.lastEndStop);
  TEST_ASSERT_EQUAL_UINT32(8, wire.txLength);
  TEST_ASSERT_EQUAL_UINT8(0, wire.tx[0]);
  for (uint8_t i = 0; i < 7; ++i) TEST_ASSERT_EQUAL_UINT8(data[i], wire.registers[i]);
}

void test_wire_io_rejects_zero_or_oversized_register_spans() {
  TwoWire wire;
  TestI2cBusLock lock;
  wire.lockHeldProbe = &lock.held;
  Esp32Ds3231RegisterIo io(wire, lock);
  uint8_t data[8] = {};
  TEST_ASSERT_FALSE(io.readRegisters(0, data, 0));
  TEST_ASSERT_FALSE(io.readRegisters(0, data, 8));
  TEST_ASSERT_EQUAL_INT(DS3231_WRITE_NOT_STARTED,
                        io.writeRegisters(0, data, 0));
  TEST_ASSERT_EQUAL_INT(DS3231_WRITE_NOT_STARTED,
                        io.writeRegisters(0, data, 8));
  TEST_ASSERT_EQUAL_UINT32(0, lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.beginCalls);
}

void test_concrete_i2c_mutex_can_be_reacquired_after_release() {
  I2cBusMutex lock;
  TEST_ASSERT_TRUE(lock.valid());
  TEST_ASSERT_TRUE(lock.acquire());
  lock.release();
  TEST_ASSERT_TRUE(lock.acquire());
  lock.release();
}

void test_bus_lock_failure_prevents_any_wire_access() {
  TwoWire wire;
  TestI2cBusLock lock;
  lock.allow = false;
  wire.lockHeldProbe = &lock.held;
  Esp32Ds3231RegisterIo io(wire, lock);
  uint8_t out[7] = {};
  const uint8_t data[7] = {1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_FALSE(io.readRegisters(0, out, 7));
  TEST_ASSERT_EQUAL_INT(DS3231_WRITE_NOT_STARTED,
                        io.writeRegisters(0, data, 7));
  TEST_ASSERT_EQUAL_UINT32(2, lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(0, lock.releaseCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.beginCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.unlockedAccessCalls);
}

void test_write_lock_failure_is_not_reported_as_uncertain() {
  TwoWire wire;
  TestI2cBusLock lock;
  lock.allow = false;
  wire.lockHeldProbe = &lock.held;
  Esp32Ds3231RegisterIo io(wire, lock);
  Ds3231Clock clock(io);
  TEST_ASSERT_EQUAL_INT(DS3231_IO_NOT_STARTED,
                        clock.writeUtcEpoch(1794371696u, 480));
  TEST_ASSERT_EQUAL_UINT32(0, wire.beginCalls);
}

void test_partial_wire_write_is_uncertain_and_prefix_can_be_visible() {
  TwoWire wire;
  TestI2cBusLock lock;
  wire.lockHeldProbe = &lock.held;
  wire.writeLimit = 3;  // register pointer plus seconds and minutes only
  wire.registers[2] = 0xAA;
  Esp32Ds3231RegisterIo io(wire, lock);
  Ds3231Clock clock(io);

  TEST_ASSERT_EQUAL_INT(DS3231_IO_UNCERTAIN,
                        clock.writeUtcEpoch(1794371696u, 480));
  TEST_ASSERT_EQUAL_UINT32(3, wire.txLength);
  TEST_ASSERT_EQUAL_UINT8(0x56, wire.registers[0]);
  TEST_ASSERT_EQUAL_UINT8(0x34, wire.registers[1]);
  TEST_ASSERT_EQUAL_UINT8(0xAA, wire.registers[2]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_read_converts_local_pht_wall_time_to_utc_epoch);
  RUN_TEST(test_read_handles_positive_offset_crossing_previous_utc_day);
  RUN_TEST(test_read_handles_negative_offset_crossing_next_utc_day);
  RUN_TEST(test_read_supports_twelve_hour_midnight_and_noon);
  RUN_TEST(test_read_accepts_gregorian_leap_day);
  RUN_TEST(test_read_rejects_february_29_in_non_leap_year);
  RUN_TEST(test_read_supports_century_bit_for_year_2100);
  RUN_TEST(test_read_rejects_february_29_in_2100);
  RUN_TEST(test_read_rejects_time_beyond_project_uint32_epoch);
  RUN_TEST(test_read_rejects_invalid_bcd_and_reserved_bits);
  RUN_TEST(test_read_rejects_bad_calendar_fields_and_hour_encoding);
  RUN_TEST(test_read_exposes_osf_without_treating_candidate_epoch_as_trusted);
  RUN_TEST(test_read_rejects_invalid_utc_offset_without_bus_access);
  RUN_TEST(test_read_io_failure_preserves_output);
  RUN_TEST(test_write_converts_utc_to_configured_local_time_and_uses_24_hour_mode);
  RUN_TEST(test_write_handles_local_midnight_rollover);
  RUN_TEST(test_write_sets_century_bit_for_year_2100);
  RUN_TEST(test_write_rejects_pre_2000_local_time_without_mutation);
  RUN_TEST(test_write_rejects_out_of_range_offset_without_mutation);
  RUN_TEST(test_write_io_failure_is_reported_as_uncertain);
  RUN_TEST(test_clear_osf_is_explicit_and_preserves_other_status_flags);
  RUN_TEST(test_clear_osf_is_noop_when_flag_is_already_clear);
  RUN_TEST(test_clear_osf_preserves_alarm_flags_latched_after_status_read);
  RUN_TEST(test_clear_osf_verifies_flag_remains_clear_after_write);
  RUN_TEST(test_clear_osf_returns_uncertain_when_verification_read_fails);
  RUN_TEST(test_clear_osf_does_not_hide_status_read_or_write_errors);
  RUN_TEST(test_read_status_io_failure_preserves_output);
  RUN_TEST(test_wire_register_read_uses_repeated_start_and_exact_length);
  RUN_TEST(test_wire_short_read_fails_without_partial_output);
  RUN_TEST(test_wire_nack_fails_register_read);
  RUN_TEST(test_wire_register_write_sends_one_bounded_transaction);
  RUN_TEST(test_wire_io_rejects_zero_or_oversized_register_spans);
  RUN_TEST(test_concrete_i2c_mutex_can_be_reacquired_after_release);
  RUN_TEST(test_bus_lock_failure_prevents_any_wire_access);
  RUN_TEST(test_write_lock_failure_is_not_reported_as_uncertain);
  RUN_TEST(test_partial_wire_write_is_uncertain_and_prefix_can_be_visible);
  return UNITY_END();
}
