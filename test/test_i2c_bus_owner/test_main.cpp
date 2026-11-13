#include <unity.h>
#include <stdint.h>

#include "esp32_i2c_bus_owner.h"
#include "Wire.h"

using namespace att;

void setUp() {}
void tearDown() {}

void test_begin_initializes_the_injected_bus_once_with_requested_pins_and_clock() {
  TwoWire wire;
  Esp32I2cBusOwner owner(wire);
  TEST_ASSERT_FALSE(owner.ready());
  TEST_ASSERT_FALSE(owner.lock().acquire());
  TEST_ASSERT_TRUE(owner.begin(21, 22, 100000UL));
  TEST_ASSERT_TRUE(owner.ready());
  TEST_ASSERT_EQUAL_UINT32(1, wire.busBeginCalls);
  TEST_ASSERT_EQUAL_INT(21, wire.busSda);
  TEST_ASSERT_EQUAL_INT(22, wire.busScl);
  TEST_ASSERT_EQUAL_UINT32(100000UL, wire.busClockHz);
  TEST_ASSERT_TRUE(owner.begin(21, 22, 100000UL));
  TEST_ASSERT_EQUAL_UINT32(1, wire.busBeginCalls);
}

void test_failed_begin_is_terminal_and_never_reinitializes_a_possibly_partial_bus() {
  TwoWire wire;
  wire.busBeginResult = false;
  Esp32I2cBusOwner owner(wire);
  TEST_ASSERT_FALSE(owner.begin(21, 22, 100000UL));
  TEST_ASSERT_FALSE(owner.ready());
  TEST_ASSERT_FALSE(owner.lock().acquire());
  wire.busBeginResult = true;
  TEST_ASSERT_FALSE(owner.begin(21, 22, 100000UL));
  TEST_ASSERT_EQUAL_UINT32(1, wire.busBeginCalls);
}

void test_invalid_pin_or_clock_arguments_do_not_touch_wire() {
  TwoWire wire;
  Esp32I2cBusOwner owner(wire);
  TEST_ASSERT_FALSE(owner.begin(-1, 22, 100000UL));
  TEST_ASSERT_FALSE(owner.begin(21, 22, 0));
  TEST_ASSERT_FALSE(owner.ready());
  TEST_ASSERT_EQUAL_UINT32(0, wire.busBeginCalls);
}

void test_clients_receive_the_same_owner_lock() {
  TwoWire wire;
  Esp32I2cBusOwner owner(wire);
  TEST_ASSERT_TRUE(owner.begin(21, 22, 100000UL));
  II2cBusLock& first = owner.lock();
  II2cBusLock& second = owner.lock();
  TEST_ASSERT_TRUE(&first == &second);
  TEST_ASSERT_TRUE(first.acquire());
  first.release();
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_initializes_the_injected_bus_once_with_requested_pins_and_clock);
  RUN_TEST(test_failed_begin_is_terminal_and_never_reinitializes_a_possibly_partial_bus);
  RUN_TEST(test_invalid_pin_or_clock_arguments_do_not_touch_wire);
  RUN_TEST(test_clients_receive_the_same_owner_lock);
  return UNITY_END();
}
