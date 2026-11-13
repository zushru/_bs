#include <unity.h>
#include <string.h>
#include <type_traits>
#include <utility>
#include "as608_sensor_arbiter.h"
#include "as608_enroll_sensor.h"
#include "stream_as608_link.h"
#include "as608_runtime.h"
#include "as608_startup.h"
using namespace att;

static_assert(std::is_same<decltype(std::declval<As608Runtime&>().occupancy()),
                           const Occupancy&>::value,
              "Runtime occupancy must be exposed read-only");
static_assert(std::is_same<decltype(std::declval<As608Runtime&>().enrollment()),
                           const As608EnrollmentSession&>::value,
              "Runtime enrollment session must be exposed read-only");
static_assert(std::is_same<decltype(std::declval<As608Runtime&>().attendance()),
                           const As608AttendanceClient&>::value,
              "Runtime attendance client must not expose raw mutating API");

uint32_t gTestMillis = 0;

struct FakeStream : Stream {
  uint8_t tx[64], rx[64];
  size_t txLength = 0, rxLength = 0, rxPos = 0;
  bool partialWrite = false;
  size_t write(const uint8_t* data, size_t length) override {
    txLength = length;
    if (txLength > sizeof tx) return 0;
    memcpy(tx, data, length);
    return partialWrite && length ? length - 1 : length;
  }
  int available() override { return (int)(rxLength - rxPos); }
  int read() override { return rxPos < rxLength ? rx[rxPos++] : -1; }
  void feed(const uint8_t* data, size_t length) {
    if (length > sizeof rx) return;
    memcpy(rx, data, length); rxLength = length; rxPos = 0;
  }
  void ackWith(uint8_t code, const uint8_t* data = nullptr, size_t dataLength = 0) {
    if (dataLength > 32 || (dataLength && !data)) return;
    uint8_t frame[64];
    const uint16_t packetLength = (uint16_t)(dataLength + 3);
    frame[0]=0xEF; frame[1]=0x01;
    frame[2]=frame[3]=frame[4]=frame[5]=0xFF;
    frame[6]=0x07; frame[7]=(uint8_t)(packetLength >> 8); frame[8]=(uint8_t)packetLength;
    frame[9]=code;
    if (dataLength) memcpy(frame + 10, data, dataLength);
    uint16_t sum = (uint16_t)(0x07 + frame[7] + frame[8] + code);
    for (size_t i=0; i<dataLength; ++i) sum = (uint16_t)(sum + data[i]);
    frame[10 + dataLength]=(uint8_t)(sum >> 8);
    frame[11 + dataLength]=(uint8_t)sum;
    feed(frame, dataLength + 12);
  }
  void ack() { ackWith(0); }
};

void setUp() {}
void tearDown() {}

void test_attendance_command_requires_an_attendance_lease() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  As608AttendanceLease lease(gate);

  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_FALSE(gate.commandInFlight());
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.release(false, false));
}

void test_idle_gate_allows_one_attendance_command() {
  As608SensorArbiter gate;
  As608AttendanceLease lease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_IO_ERROR, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_TRUE(gate.commandInFlight());
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.release(false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_enrollment_reservation_blocks_attendance_but_allows_enrollment() {
  As608SensorArbiter gate;
  As608AttendanceLease attendanceLease(gate);
  As608EnrollmentLease lease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_FALSE(gate.attendanceAllowed());
  TEST_ASSERT_FALSE(attendanceLease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ENROLLMENT, request, sizeof request, 50));
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.abortPreparation(false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_reservation_cannot_release_while_command_is_in_flight() {
  As608SensorArbiter gate;
  As608EnrollmentLease lease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ENROLLMENT, request, sizeof request, 50));
  TEST_ASSERT_FALSE(lease.abortPreparation(false, false));
  TEST_ASSERT_FALSE(gate.attendanceAllowed());
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.abortPreparation(false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_wrong_owner_cannot_finish_another_owners_command() {
  As608SensorArbiter gate;
  As608AttendanceLease lease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(gate.commandInFlight());
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.release(false, false));
}

void test_enrollment_commands_require_a_reservation() {
  As608SensorArbiter gate;
  As608EnrollmentLease lease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.startTransaction(AS608_OWNER_ENROLLMENT, request, sizeof request, 50));
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ENROLLMENT, request, sizeof request, 50));
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.abortPreparation(false, false));
}

void test_lease_does_not_release_during_cancel_fence_or_release_fault() {
  As608SensorArbiter gate;
  As608EnrollmentLease lease(gate);
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_FALSE(lease.finish(EN_CANCEL_PENDING_RELEASE, false, false));
  TEST_ASSERT_FALSE(lease.finish(EN_CANCEL_RELEASE_FAULT, false, false));
  TEST_ASSERT_FALSE(gate.attendanceAllowed());
  TEST_ASSERT_TRUE(lease.finish(EN_CANCELLED, false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_lease_requires_terminal_state_and_drained_sensor_and_reader() {
  As608SensorArbiter gate;
  As608EnrollmentLease lease(gate);
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_FALSE(lease.finish(EN_DONE, true, false));
  TEST_ASSERT_FALSE(lease.finish(EN_DONE, false, true));
  TEST_ASSERT_FALSE(lease.finish(EN_WAIT_FIRST_FINGER, false, false));
  TEST_ASSERT_FALSE(gate.attendanceAllowed());
  TEST_ASSERT_TRUE(lease.finish(EN_DONE, false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_setup_abort_releases_only_after_all_sensor_work_is_drained() {
  As608SensorArbiter gate;
  As608EnrollmentLease lease(gate);
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_FALSE(lease.abortPreparation(true, false));
  TEST_ASSERT_FALSE(lease.abortPreparation(false, true));
  TEST_ASSERT_FALSE(gate.attendanceAllowed());
  TEST_ASSERT_TRUE(lease.abortPreparation(false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_lease_cannot_be_acquired_twice_or_release_another_lease() {
  As608SensorArbiter gate;
  As608EnrollmentLease first(gate), second(gate);
  TEST_ASSERT_TRUE(first.acquire());
  TEST_ASSERT_FALSE(first.acquire());
  TEST_ASSERT_FALSE(second.acquire());
  TEST_ASSERT_FALSE(second.abortPreparation(false, false));
  TEST_ASSERT_TRUE(first.abortPreparation(false, false));
  TEST_ASSERT_TRUE(second.acquire());
}


struct NullLink : IAs608Link {
  int startTransaction(As608CommandOwner, const uint8_t*, size_t, uint32_t) override {
    return AS608_LINK_IO_ERROR;
  }
  int pollTransaction(As608CommandOwner, uint8_t*, size_t, size_t&) override {
    return AS608_LINK_IO_ERROR;
  }
};

struct FaultedLink : NullLink {
  bool faulted() const override { return true; }
};

void test_session_holds_gate_through_setup_and_releases_only_after_fsm_terminal() {
  As608SensorArbiter gate;
  NullLink link;
  As608EnrollSensor sensor(link, 0);
  RosterStore roster(480);
  Occupancy occupancy;
  occupancy.finish();
  EnrollFsm fsm(roster, occupancy, sensor);
  As608OccupancyReader reader(sensor, occupancy);
  As608EnrollmentSession session(gate, fsm, sensor, reader);
  FakeStream stream;
  StreamAs608Link transport(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;

  As608AttendanceLease attendanceLease(gate);
  TEST_ASSERT_TRUE(session.acquire());
  TEST_ASSERT_FALSE(attendanceLease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, transport.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, transport.startTransaction(AS608_OWNER_ENROLLMENT, request, sizeof request, 50));
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, transport.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_NO_WORK, fsm.begin(300, false));
  TEST_ASSERT_TRUE(session.sync());
  TEST_ASSERT_FALSE(session.held());
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_session_aborts_setup_only_while_fsm_is_idle_and_reader_drained() {
  As608SensorArbiter gate;
  NullLink link;
  As608EnrollSensor sensor(link, 0);
  RosterStore roster(480);
  Occupancy occupancy;
  EnrollFsm fsm(roster, occupancy, sensor);
  As608OccupancyReader reader(sensor, occupancy);
  As608EnrollmentSession session(gate, fsm, sensor, reader);
  TEST_ASSERT_TRUE(session.acquire());
  TEST_ASSERT_TRUE(session.abortPreparation());
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}




void test_stream_link_holds_command_lease_until_ack_is_consumed() {
  As608SensorArbiter gate;
  As608AttendanceLease lease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  gTestMillis = 100;
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_TRUE(gate.commandInFlight());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_PENDING, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_EQUAL_UINT32(12, responseLength);
  TEST_ASSERT_FALSE(gate.commandInFlight());
  TEST_ASSERT_TRUE(lease.release(false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_stream_link_enforces_enrollment_reservation_and_owner_match() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  As608EnrollmentLease lease(gate);
  TEST_ASSERT_TRUE(lease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ENROLLMENT, request, sizeof request, 50));
  TEST_ASSERT_EQUAL_INT(AS608_LINK_BUSY, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(gate.commandInFlight());
  stream.ack();
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.pollTransaction(AS608_OWNER_ENROLLMENT, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(lease.abortPreparation(false, false));
  TEST_ASSERT_TRUE(gate.attendanceAllowed());
}

void test_stream_link_partial_write_fault_releases_command_lease() {
  As608SensorArbiter gate;
  As608AttendanceLease attendanceLease(gate);
  As608EnrollmentLease enrollmentLease(gate);
  FakeStream stream; stream.partialWrite = true;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  TEST_ASSERT_TRUE(attendanceLease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_IO_ERROR, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  TEST_ASSERT_TRUE(link.faulted());
  TEST_ASSERT_FALSE(gate.commandInFlight());
  TEST_ASSERT_TRUE(attendanceLease.held());
  TEST_ASSERT_FALSE(attendanceLease.release(false, link.faulted()));
  TEST_ASSERT_FALSE(enrollmentLease.acquire());
}

void test_stream_link_timeout_fault_releases_command_lease() {
  As608SensorArbiter gate;
  As608AttendanceLease attendanceLease(gate);
  As608EnrollmentLease enrollmentLease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  uint8_t response[64]; size_t responseLength = 0;
  gTestMillis = 100;
  TEST_ASSERT_TRUE(attendanceLease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 5));
  gTestMillis = 105;
  TEST_ASSERT_EQUAL_INT(AS608_LINK_TIMEOUT, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(link.faulted());
  TEST_ASSERT_FALSE(gate.commandInFlight());
  TEST_ASSERT_TRUE(attendanceLease.held());
  TEST_ASSERT_FALSE(attendanceLease.release(false, link.faulted()));
  TEST_ASSERT_FALSE(enrollmentLease.acquire());
}


void test_stream_link_malformed_and_oversize_frames_fault_closed() {
  const uint8_t malformed[9] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x07,0x00,0x02};
  const uint8_t oversized[9] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x07,0x00,0x38};
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  for (uint8_t which = 0; which < 2; ++which) {
    As608SensorArbiter gate;
    As608AttendanceLease attendanceLease(gate);
    As608EnrollmentLease enrollmentLease(gate);
    FakeStream stream;
    StreamAs608Link link(stream, gate);
    uint8_t response[64]; size_t responseLength = 0;
    gTestMillis = 200;
    TEST_ASSERT_TRUE(attendanceLease.acquire());
    TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
    stream.feed(which == 0 ? malformed : oversized, 9);
    const int expected = which == 0 ? AS608_LINK_BAD_FRAME : AS608_LINK_OVERFLOW;
    TEST_ASSERT_EQUAL_INT(expected, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
    TEST_ASSERT_TRUE(link.faulted());
    TEST_ASSERT_FALSE(gate.commandInFlight());
    TEST_ASSERT_TRUE(attendanceLease.held());
    TEST_ASSERT_FALSE(attendanceLease.release(false, link.faulted()));
    TEST_ASSERT_FALSE(enrollmentLease.acquire());
  }
}

void test_stream_link_response_buffer_overflow_releases_lease() {
  As608SensorArbiter gate;
  As608AttendanceLease attendanceLease(gate);
  As608EnrollmentLease enrollmentLease(gate);
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  const uint8_t request[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x01,0x00,0x05};
  const uint8_t responseFrame[13] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x07,0x00,0x04,0x00,0xAA,0x00,0xB5};
  uint8_t response[12]; size_t responseLength = 0;
  gTestMillis = 300;
  TEST_ASSERT_TRUE(attendanceLease.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OK, link.startTransaction(AS608_OWNER_ATTENDANCE, request, sizeof request, 50));
  stream.feed(responseFrame, sizeof responseFrame);
  TEST_ASSERT_EQUAL_INT(AS608_LINK_OVERFLOW, link.pollTransaction(AS608_OWNER_ATTENDANCE, response, sizeof response, responseLength));
  TEST_ASSERT_TRUE(link.faulted());
  TEST_ASSERT_FALSE(gate.commandInFlight());
  TEST_ASSERT_TRUE(attendanceLease.held());
  TEST_ASSERT_FALSE(attendanceLease.release(false, link.faulted()));
  TEST_ASSERT_FALSE(enrollmentLease.acquire());
}


void test_enrollment_adapter_cannot_send_without_session_reservation() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link transport(stream, gate);
  As608EnrollSensor sensor(transport, 0);
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.authenticate());
  TEST_ASSERT_EQUAL_UINT32(0, stream.txLength);
  TEST_ASSERT_FALSE(gate.commandInFlight());
}


static void setupAttendance(As608AttendanceClient& attendance, FakeStream& stream) {
  TEST_ASSERT_TRUE(attendance.beginSetup());
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepSetup());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.tx[9]);
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepSetup());
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepSetup());
  TEST_ASSERT_EQUAL_UINT8(0x0F, stream.tx[9]);
  uint8_t params[16] = {0};
  params[4] = 0x01; params[5] = 0x2C;  // capacity 300
  stream.ackWith(0x00, params, sizeof params);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_READY, attendance.stepSetup());
  TEST_ASSERT_TRUE(attendance.ready());
  TEST_ASSERT_FALSE(attendance.held());
}

void test_attendance_setup_uses_attendance_owner_and_holds_lease_across_auth_and_capacity() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  As608AttendanceClient attendance(gate, link, 0x12345678UL);
  As608EnrollmentLease enrollment(gate);
  TEST_ASSERT_TRUE(attendance.beginSetup());
  TEST_ASSERT_FALSE(enrollment.acquire());
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepSetup());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.tx[9]);
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepSetup());
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepSetup());
  TEST_ASSERT_EQUAL_UINT8(0x0F, stream.tx[9]);
  uint8_t params[16] = {0}; params[4]=0x01; params[5]=0x2C;
  stream.ackWith(0x00, params, sizeof params);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_READY, attendance.stepSetup());
  TEST_ASSERT_TRUE(attendance.ready());
  TEST_ASSERT_FALSE(attendance.held());
  TEST_ASSERT_TRUE(enrollment.acquire());
  TEST_ASSERT_TRUE(enrollment.abortPreparation(false, false));
}

void test_attendance_scan_lease_prevents_enrollment_between_sensor_commands() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  As608AttendanceClient attendance(gate, link, 0);
  As608EnrollmentLease enrollment(gate);
  setupAttendance(attendance, stream);

  TEST_ASSERT_TRUE(attendance.beginScan());
  uint16_t page = 0xFFFF, score = 0;
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_EQUAL_UINT8(0x01, stream.tx[9]);
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_FALSE(enrollment.acquire());

  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_EQUAL_UINT8(0x02, stream.tx[9]);
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_FALSE(enrollment.acquire());

  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_EQUAL_UINT8(0x04, stream.tx[9]);
  const uint8_t match[4] = {0x00,0x07,0x01,0x23};
  stream.ackWith(0x00, match, sizeof match);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_MATCH, attendance.stepScan(page, score));
  TEST_ASSERT_EQUAL_UINT32(7, page);  // physical page, not an ID conversion
  TEST_ASSERT_EQUAL_UINT32(0x0123, score);
  TEST_ASSERT_FALSE(attendance.held());
  TEST_ASSERT_TRUE(enrollment.acquire());
  TEST_ASSERT_TRUE(enrollment.abortPreparation(false, false));
}

void test_runtime_enrollment_session_cannot_acquire_before_startup() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  TEST_ASSERT_FALSE(runtime.enrollment().available());
  TEST_ASSERT_FALSE(runtime.enrollment().held());
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_FALSE(runtime.attendance().active());
  TEST_ASSERT_FALSE(runtime.attendance().held());
  TEST_ASSERT_EQUAL_UINT32(0, stream.txLength);
  As608StartupCoordinator startup(runtime);
  TEST_ASSERT_TRUE(startup.begin());
  TEST_ASSERT_FALSE(runtime.enrollment().available());
  TEST_ASSERT_TRUE(runtime.enrollment().held());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.tx[9]);
}

void test_runtime_attendance_setup_cannot_bypass_startup_coordinator() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);

  TEST_ASSERT_EQUAL_UINT32(0, stream.txLength);
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_FALSE(runtime.attendance().active());
  TEST_ASSERT_FALSE(runtime.attendance().held());
}

void test_attendance_transport_fault_keeps_sensor_reserved_fail_closed() {
  As608SensorArbiter gate;
  FakeStream stream; stream.partialWrite = true;
  StreamAs608Link link(stream, gate);
  As608AttendanceClient attendance(gate, link, 0);
  As608EnrollmentLease enrollment(gate);
  TEST_ASSERT_TRUE(attendance.beginSetup());
  TEST_ASSERT_EQUAL_INT(AS608_ATT_FATAL, attendance.stepSetup());
  TEST_ASSERT_TRUE(attendance.faulted());
  TEST_ASSERT_TRUE(attendance.held());
  TEST_ASSERT_FALSE(enrollment.acquire());
  TEST_ASSERT_EQUAL_UINT32(0, stream.rxLength);
}


void test_attendance_no_finger_releases_scan_lease_cleanly() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  As608AttendanceClient attendance(gate, link, 0);
  As608EnrollmentLease enrollment(gate);
  setupAttendance(attendance, stream);
  TEST_ASSERT_TRUE(attendance.beginScan());
  uint16_t page = 0, score = 0;
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  stream.ackWith(0x02);  // no finger
  TEST_ASSERT_EQUAL_INT(AS608_ATT_NO_FINGER, attendance.stepScan(page, score));
  TEST_ASSERT_FALSE(attendance.held());
  TEST_ASSERT_TRUE(enrollment.acquire());
  TEST_ASSERT_TRUE(enrollment.abortPreparation(false, false));
}

void test_attendance_no_match_releases_scan_lease_cleanly() {
  As608SensorArbiter gate;
  FakeStream stream;
  StreamAs608Link link(stream, gate);
  As608AttendanceClient attendance(gate, link, 0);
  As608EnrollmentLease enrollment(gate);
  setupAttendance(attendance, stream);
  TEST_ASSERT_TRUE(attendance.beginScan());
  uint16_t page = 0, score = 0;
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  TEST_ASSERT_EQUAL_INT(AS608_ATT_PENDING, attendance.stepScan(page, score));
  const uint8_t noMatchPageAndScore[4] = {0,0,0,0};
  stream.ackWith(0x09, noMatchPageAndScore, sizeof noMatchPageAndScore);
  TEST_ASSERT_EQUAL_INT(AS608_ATT_NO_MATCH, attendance.stepScan(page, score));
  TEST_ASSERT_FALSE(attendance.held());
  TEST_ASSERT_TRUE(enrollment.acquire());
  TEST_ASSERT_TRUE(enrollment.abortPreparation(false, false));
}


void test_enrollment_session_refuses_setup_abort_after_transport_poison() {
  As608SensorArbiter gate;
  FaultedLink link;
  As608EnrollSensor sensor(link, 0);
  RosterStore roster(480);
  Occupancy occupancy;
  EnrollFsm fsm(roster, occupancy, sensor);
  As608OccupancyReader reader(sensor, occupancy);
  As608EnrollmentSession session(gate, fsm, sensor, reader);
  TEST_ASSERT_TRUE(session.acquire());
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.authenticate());
  TEST_ASSERT_TRUE(sensor.linkFaulted());
  TEST_ASSERT_FALSE(session.abortPreparation());
  TEST_ASSERT_FALSE(gate.attendanceAllowed());
  TEST_ASSERT_TRUE(session.held());
}

static bool startupReachSecondIndexPage(As608StartupCoordinator& startup, FakeStream& stream) {
  if (!startup.begin()) return false;
  if (startup.step() != AS608_STARTUP_PENDING || stream.tx[9] != 0x13) return false;
  stream.ackWith(0x00);
  if (startup.step() != AS608_STARTUP_PENDING) return false;
  if (startup.step() != AS608_STARTUP_PENDING || stream.tx[9] != 0x0F) return false;
  uint8_t params[16] = {0};
  params[4] = 0x01; params[5] = 0x2C;
  stream.ackWith(0x00, params, sizeof params);
  if (startup.step() != AS608_STARTUP_PENDING) return false;
  if (startup.step() != AS608_STARTUP_PENDING ||
      stream.tx[9] != 0x1F || stream.tx[10] != 0) return false;
  uint8_t firstPage[32] = {0};
  firstPage[0] = 0x01;  // synthetic slot 0 occupancy, not a hardware observation
  stream.ackWith(0x00, firstPage, sizeof firstPage);
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // bounded 32-byte RX poll
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // finish the 44-byte ACK
  if (startup.step() != AS608_STARTUP_PENDING ||
      stream.tx[9] != 0x1F || stream.tx[10] != 1) return false;
  return true;
}

static bool startupFinishIndexAndStartAttendance(As608StartupCoordinator& startup,
                                                  FakeStream& stream) {
  uint8_t secondPage[32] = {0};
  secondPage[0] = 0x01;  // synthetic slot 256 occupancy
  stream.ackWith(0x00, secondPage, sizeof secondPage);
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // bounded 32-byte RX poll
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // finish ACK, release enrollment lease
  return startup.ready() == false;
}

static bool startupDriveAttendanceCapacity(As608StartupCoordinator& startup,
                                            FakeStream& stream, uint16_t capacity) {
  if (startup.step() != AS608_STARTUP_PENDING || stream.tx[9] != 0x13) return false;
  stream.ackWith(0x00);
  if (startup.step() != AS608_STARTUP_PENDING) return false;
  if (startup.step() != AS608_STARTUP_PENDING || stream.tx[9] != 0x0F) return false;
  uint8_t params[16] = {0};
  params[4] = (uint8_t)(capacity >> 8);
  params[5] = (uint8_t)capacity;
  stream.ackWith(0x00, params, sizeof params);
  return true;
}

void test_runtime_owned_occupancy_is_refreshed_without_external_map_reference() {
  FakeStream stream;
  RosterStore roster(480);
  As608Runtime runtime(stream, roster, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_FALSE(runtime.occupancy().valid);
  TEST_ASSERT_FALSE(runtime.enrollment().available());
  TEST_ASSERT_TRUE(startupReachSecondIndexPage(startup, stream));
  TEST_ASSERT_TRUE(startupFinishIndexAndStartAttendance(startup, stream));
  TEST_ASSERT_TRUE(runtime.occupancy().valid);
  TEST_ASSERT_TRUE(runtime.occupancy().has(1));
  TEST_ASSERT_TRUE(runtime.occupancy().has(257));
  TEST_ASSERT_TRUE(startupDriveAttendanceCapacity(startup, stream, 300));
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_READY, startup.step());
  TEST_ASSERT_TRUE(runtime.occupancy().valid);
  TEST_ASSERT_TRUE(runtime.attendance().ready());
  TEST_ASSERT_TRUE(runtime.enrollment().available());
}

void test_startup_refreshes_complete_occupancy_before_enabling_attendance() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  occupancy.setPresent(1, true);
  occupancy.finish();  // stale pre-startup state must not survive a new sensor refresh
  TEST_ASSERT_TRUE(occupancy.valid);
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_TRUE(startup.begin());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_FALSE(runtime.attendance().active());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.tx[9]);
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x0F, stream.tx[9]);
  uint8_t params[16] = {0};
  params[4] = 0x01; params[5] = 0x2C;
  stream.ackWith(0x00, params, sizeof params);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x1F, stream.tx[9]);
  TEST_ASSERT_EQUAL_UINT8(0, stream.tx[10]);

  uint8_t firstPage[32] = {0}; firstPage[0] = 0x01;
  stream.ackWith(0x00, firstPage, sizeof firstPage);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());  // bounded 32-byte RX poll
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_TRUE(runtime.enrollment().held());
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());  // finish 44-byte ACK
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());  // issue page 1
  TEST_ASSERT_EQUAL_UINT8(0x1F, stream.tx[9]);
  TEST_ASSERT_EQUAL_UINT8(1, stream.tx[10]);

  uint8_t secondPage[32] = {0}; secondPage[0] = 0x01;
  stream.ackWith(0x00, secondPage, sizeof secondPage);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());  // bounded 32-byte RX poll
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());  // finish 44-byte ACK and start attendance setup
  TEST_ASSERT_TRUE(occupancy.valid);
  TEST_ASSERT_TRUE(occupancy.has(1));
  TEST_ASSERT_TRUE(occupancy.has(257));
  TEST_ASSERT_FALSE(runtime.enrollment().held());
  TEST_ASSERT_FALSE(runtime.attendance().ready());

  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.tx[9]);
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x0F, stream.tx[9]);
  stream.ackWith(0x00, params, sizeof params);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_READY, startup.step());
  TEST_ASSERT_TRUE(startup.ready());
  TEST_ASSERT_EQUAL_UINT16(300, startup.capacity());
  TEST_ASSERT_TRUE(runtime.attendance().ready());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_READY, startup.step());
  TEST_ASSERT_TRUE(occupancy.valid);
  TEST_ASSERT_TRUE(runtime.attendance().ready());
  TEST_ASSERT_TRUE(runtime.enrollment().available());
  TEST_ASSERT_FALSE(runtime.enrollment().held());
}

void test_startup_coordinator_is_only_runtime_attendance_setup_path() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_TRUE(startupReachSecondIndexPage(startup, stream));
  TEST_ASSERT_TRUE(startupFinishIndexAndStartAttendance(startup, stream));
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_TRUE(runtime.attendance().active());
  TEST_ASSERT_TRUE(runtime.attendance().held());
  TEST_ASSERT_TRUE(startupDriveAttendanceCapacity(startup, stream, 300));
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_READY, startup.step());
  TEST_ASSERT_TRUE(startup.ready());
  TEST_ASSERT_TRUE(runtime.attendance().ready());
  TEST_ASSERT_FALSE(runtime.attendance().held());
}

void test_startup_partial_index_failure_invalidates_occupancy_and_never_enables_scan() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_TRUE(startupReachSecondIndexPage(startup, stream));
  // Page 0 has already been ACKed by startupReachSecondIndexPage().
  TEST_ASSERT_FALSE(occupancy.valid);
  stream.ackWith(0x06);  // non-OK index-page reply after a partial refresh
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_FAULT, startup.step());
  TEST_ASSERT_FALSE(startup.ready());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_FALSE(runtime.enrollment().held());
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_EQUAL_UINT8(0x1F, stream.tx[9]);
}

void test_startup_transport_fault_keeps_reservation_and_blocks_attendance() {
  FakeStream stream;
  stream.partialWrite = true;
  RosterStore roster(480);
  Occupancy occupancy;
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_TRUE(startup.begin());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_FAULT, startup.step());
  TEST_ASSERT_TRUE(startup.faulted());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_TRUE(runtime.transportFaulted());
  TEST_ASSERT_TRUE(runtime.enrollment().held());
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_FALSE(startup.begin());
}

void test_startup_authentication_rejection_releases_setup_lease() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  occupancy.setPresent(1, true);
  occupancy.finish();
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_TRUE(startup.begin());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, startup.step());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.tx[9]);
  stream.ackWith(0x13);  // non-OK verify-password ACK
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_FAULT, startup.step());
  TEST_ASSERT_TRUE(startup.faulted());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_FALSE(runtime.enrollment().held());
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_FALSE(startup.begin());  // faulted startup cannot be reused
}

void test_startup_capacity_disagreement_disables_attendance_again() {
  FakeStream stream;
  RosterStore roster(480);
  Occupancy occupancy;
  As608Runtime runtime(stream, roster, occupancy, 0x12345678UL);
  As608StartupCoordinator startup(runtime);

  TEST_ASSERT_TRUE(startupReachSecondIndexPage(startup, stream));
  TEST_ASSERT_TRUE(startupFinishIndexAndStartAttendance(startup, stream));
  TEST_ASSERT_TRUE(occupancy.valid);
  TEST_ASSERT_FALSE(runtime.attendance().ready());
  TEST_ASSERT_TRUE(startupDriveAttendanceCapacity(startup, stream, 299));
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_FAULT, startup.step());
  TEST_ASSERT_FALSE(startup.ready());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_FALSE(runtime.attendance().ready());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_attendance_command_requires_an_attendance_lease);
  RUN_TEST(test_idle_gate_allows_one_attendance_command);
  RUN_TEST(test_enrollment_reservation_blocks_attendance_but_allows_enrollment);
  RUN_TEST(test_reservation_cannot_release_while_command_is_in_flight);
  RUN_TEST(test_wrong_owner_cannot_finish_another_owners_command);
  RUN_TEST(test_enrollment_commands_require_a_reservation);
  RUN_TEST(test_lease_does_not_release_during_cancel_fence_or_release_fault);
  RUN_TEST(test_lease_requires_terminal_state_and_drained_sensor_and_reader);
  RUN_TEST(test_setup_abort_releases_only_after_all_sensor_work_is_drained);
  RUN_TEST(test_lease_cannot_be_acquired_twice_or_release_another_lease);
  RUN_TEST(test_session_holds_gate_through_setup_and_releases_only_after_fsm_terminal);
  RUN_TEST(test_session_aborts_setup_only_while_fsm_is_idle_and_reader_drained);
  RUN_TEST(test_stream_link_holds_command_lease_until_ack_is_consumed);
  RUN_TEST(test_stream_link_enforces_enrollment_reservation_and_owner_match);
  RUN_TEST(test_stream_link_partial_write_fault_releases_command_lease);
  RUN_TEST(test_stream_link_timeout_fault_releases_command_lease);
  RUN_TEST(test_stream_link_malformed_and_oversize_frames_fault_closed);
  RUN_TEST(test_stream_link_response_buffer_overflow_releases_lease);
  RUN_TEST(test_enrollment_adapter_cannot_send_without_session_reservation);
  RUN_TEST(test_attendance_setup_uses_attendance_owner_and_holds_lease_across_auth_and_capacity);
  RUN_TEST(test_attendance_scan_lease_prevents_enrollment_between_sensor_commands);
  RUN_TEST(test_runtime_enrollment_session_cannot_acquire_before_startup);
  RUN_TEST(test_runtime_attendance_setup_cannot_bypass_startup_coordinator);
  RUN_TEST(test_attendance_transport_fault_keeps_sensor_reserved_fail_closed);
  RUN_TEST(test_attendance_no_finger_releases_scan_lease_cleanly);
  RUN_TEST(test_attendance_no_match_releases_scan_lease_cleanly);
  RUN_TEST(test_enrollment_session_refuses_setup_abort_after_transport_poison);
  RUN_TEST(test_runtime_owned_occupancy_is_refreshed_without_external_map_reference);
  RUN_TEST(test_startup_refreshes_complete_occupancy_before_enabling_attendance);
  RUN_TEST(test_startup_coordinator_is_only_runtime_attendance_setup_path);
  RUN_TEST(test_startup_partial_index_failure_invalidates_occupancy_and_never_enables_scan);
  RUN_TEST(test_startup_capacity_disagreement_disables_attendance_again);
  RUN_TEST(test_startup_transport_fault_keeps_reservation_and_blocks_attendance);
  RUN_TEST(test_startup_authentication_rejection_releases_setup_lease);
  return UNITY_END();
}
