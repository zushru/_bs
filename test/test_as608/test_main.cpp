#include <unity.h>
#include <string.h>
#include "as608_enroll_sensor.h"
using namespace att;

void setUp() {}
void tearDown() {}

struct FakeAs608Link : IAs608Link {
  uint8_t tx[64], rx[64];
  size_t txLength = 0, rxLength = 0;
  int startResult = AS608_LINK_OK;
  int pollResult = AS608_LINK_OK;
  int pendingPolls = 0;
  int starts = 0, polls = 0;
  int startTransaction(As608CommandOwner, const uint8_t* request, size_t requestLength, uint32_t) override {
    ++starts;
    txLength = requestLength;
    if (txLength > sizeof tx) return AS608_LINK_OVERFLOW;
    memcpy(tx, request, txLength);
    return startResult;
  }
  int pollTransaction(As608CommandOwner, uint8_t* response, size_t responseCapacity,
                      size_t& responseLength) override {
    ++polls;
    if (pendingPolls > 0) { --pendingPolls; return AS608_LINK_PENDING; }
    if (pollResult != AS608_LINK_OK) return pollResult;
    if (rxLength > responseCapacity) return AS608_LINK_OVERFLOW;
    memcpy(response, rx, rxLength);
    responseLength = rxLength;
    return AS608_LINK_OK;
  }
  void ack(uint8_t code, const uint8_t* data = nullptr, size_t dataLength = 0) {
    const uint16_t length = (uint16_t)(dataLength + 3);
    rx[0] = 0xEF; rx[1] = 0x01;
    rx[2] = 0xFF; rx[3] = 0xFF; rx[4] = 0xFF; rx[5] = 0xFF;
    rx[6] = 0x07; rx[7] = (uint8_t)(length >> 8); rx[8] = (uint8_t)length;
    rx[9] = code;
    if (dataLength) memcpy(rx + 10, data, dataLength);
    uint16_t sum = 0;
    for (size_t i = 6; i < 10 + dataLength; ++i) sum = (uint16_t)(sum + rx[i]);
    rx[10 + dataLength] = (uint8_t)(sum >> 8);
    rx[11 + dataLength] = (uint8_t)sum;
    rxLength = 12 + dataLength;
  }
};

static void authenticate(As608EnrollSensor& sensor, FakeAs608Link& link) {
  link.ack(0);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.authenticate());
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.authenticate());
}
static void setCapacity(As608EnrollSensor& sensor, FakeAs608Link& link, uint16_t capacity) {
  uint8_t p[16] = {0};
  p[4] = (uint8_t)(capacity >> 8); p[5] = (uint8_t)capacity;
  p[12] = 0; p[13] = 2; p[14] = 0; p[15] = 6;
  link.ack(0, p, sizeof p);
  uint16_t got = 0;
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.readCapacity(got));
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.readCapacity(got));
  TEST_ASSERT_EQUAL_UINT16(capacity, got);
}

void test_authenticate_sends_verify_password_only() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0x01020304UL);
  link.ack(0);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.authenticate());
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.authenticate());
  const uint8_t expected[] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,
                              0x13,0x01,0x02,0x03,0x04,0x00,0x25};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, link.tx, sizeof expected);
  TEST_ASSERT_EQUAL_UINT32(1, link.starts);
}

void test_read_capacity_parses_system_parameters() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link);
  setCapacity(sensor, link, 300);
  TEST_ASSERT_EQUAL_UINT16(300, sensor.capacity());
}

void test_capture_maps_no_finger_and_bad_image_contextually() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link);
  link.ack(0x02); TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage()); TEST_ASSERT_EQUAL_INT(EIO_NO_FINGER, sensor.captureImage());
  link.ack(0x06); TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage()); TEST_ASSERT_EQUAL_INT(EIO_BAD_IMAGE, sensor.captureImage());
  link.ack(0);    TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage()); TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.captureImage());
}

void test_convert_validates_buffer_and_maps_image_errors() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link);
  const int before = link.starts;
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.convertImage(3));
  TEST_ASSERT_EQUAL_INT(before, link.starts);
  link.ack(0x07); TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.convertImage(2));
  TEST_ASSERT_EQUAL_INT(EIO_BAD_IMAGE, sensor.convertImage(2));
  TEST_ASSERT_EQUAL_UINT8(2, link.tx[10]);
}

void test_search_uses_full_capacity_and_returns_physical_page() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  const uint8_t result[] = {0x01,0x2B,0x12,0x34};
  link.ack(0, result, sizeof result);
  uint16_t page = 0xFFFF, score = 0;
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.searchTemplate(page, score));
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.searchTemplate(page, score));
  TEST_ASSERT_EQUAL_UINT16(299, page);
  TEST_ASSERT_EQUAL_UINT16(0x1234, score);
  TEST_ASSERT_EQUAL_UINT8(0x04, link.tx[9]);
  TEST_ASSERT_EQUAL_UINT8(0x01, link.tx[10]);
  TEST_ASSERT_EQUAL_UINT8(0x00, link.tx[11]); TEST_ASSERT_EQUAL_UINT8(0x00, link.tx[12]);
  TEST_ASSERT_EQUAL_UINT8(0x01, link.tx[13]); TEST_ASSERT_EQUAL_UINT8(0x2C, link.tx[14]);
}

void test_no_match_is_only_a_search_result() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  link.ack(0x09);
  uint16_t page = 0, score = 0;
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.searchTemplate(page, score));
  TEST_ASSERT_EQUAL_INT(EIO_NO_MATCH, sensor.searchTemplate(page, score));
  link.ack(0x09);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage());
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.captureImage());
}

void test_search_rejects_page_outside_reported_capacity() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  const uint8_t result[] = {0x01,0x2C,0,1};
  link.ack(0, result, sizeof result);
  uint16_t page = 0, score = 0;
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.searchTemplate(page, score));
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.searchTemplate(page, score));
}

void test_create_model_maps_fingerprint_mismatch() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link);
  link.ack(0x0A);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.createModel());
  TEST_ASSERT_EQUAL_INT(EIO_MISMATCH, sensor.createModel());
  TEST_ASSERT_EQUAL_UINT8(0x05, link.tx[9]);
}

void test_store_uses_physical_page_and_never_retries_non_ok() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  link.ack(0);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.storeModel(299));
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.storeModel(299));
  TEST_ASSERT_EQUAL_UINT8(0x06, link.tx[9]);
  TEST_ASSERT_EQUAL_UINT8(0x01, link.tx[10]);
  TEST_ASSERT_EQUAL_UINT8(0x01, link.tx[11]); TEST_ASSERT_EQUAL_UINT8(0x2B, link.tx[12]);
  link.ack(0x18);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.storeModel(299));
  TEST_ASSERT_EQUAL_INT(EIO_RETRYABLE, sensor.storeModel(299));
  const int before = link.starts;
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.storeModel(300));
  TEST_ASSERT_EQUAL_INT(before, link.starts);
}

void test_bad_response_checksum_is_retryable_not_no_match() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link);
  link.ack(0);
  link.rx[link.rxLength - 1] ^= 0x01;
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage());
  TEST_ASSERT_EQUAL_INT(EIO_RETRYABLE, sensor.captureImage());
}


void test_command_polls_without_resending_packet() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link);
  link.ack(0);
  link.pendingPolls = 2;
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage());  // starts request
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage());  // poll 1
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage());  // poll 2
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.captureImage());       // final response
  TEST_ASSERT_EQUAL_UINT32(2, link.starts); // one auth + one capture
}


void test_transport_timeout_and_poisoned_link_are_fatal() {
  {
    FakeAs608Link link; As608EnrollSensor sensor(link, 0);
    link.startResult = AS608_LINK_POISONED;
    TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.authenticate());
    TEST_ASSERT_EQUAL_UINT32(1, link.starts);
  }
  {
    FakeAs608Link link; As608EnrollSensor sensor(link, 0);
    authenticate(sensor, link);
    link.pollResult = AS608_LINK_TIMEOUT;
    TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.captureImage());
    TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.captureImage());
    TEST_ASSERT_EQUAL_UINT32(2, link.starts);  // auth + one capture; timeout is not retried
  }
}


void test_read_index_page_handles_32_byte_bitmap_and_rejects_bad_page() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  uint8_t expected[32];
  for (uint8_t i = 0; i < 32; ++i) expected[i] = (uint8_t)(i * 7u + 3u);
  link.ack(0, expected, sizeof expected);
  uint8_t actual[32] = {0};
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.readIndexPage(0, actual));
  TEST_ASSERT_EQUAL_INT(EIO_OK, sensor.readIndexPage(0, actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, sizeof expected);
  TEST_ASSERT_EQUAL_UINT8(0x1F, link.tx[9]);
  TEST_ASSERT_EQUAL_UINT8(0, link.tx[10]);
  TEST_ASSERT_EQUAL_UINT32(13, link.txLength);
  TEST_ASSERT_EQUAL_UINT8(0, link.tx[7]); TEST_ASSERT_EQUAL_UINT8(4, link.tx[8]);
  TEST_ASSERT_EQUAL_UINT8(0, link.tx[11]); TEST_ASSERT_EQUAL_UINT8(0x24, link.tx[12]);
  link.ack(0, expected, sizeof expected - 1); // a short bitmap must never be accepted
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, sensor.readIndexPage(1, actual));
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.readIndexPage(1, actual));
  const int before = link.starts;
  TEST_ASSERT_EQUAL_INT(EIO_FATAL, sensor.readIndexPage(2, actual)); // 300 slots = pages 0 and 1
  TEST_ASSERT_EQUAL_INT(before, link.starts);
}

void test_occupancy_reader_validates_only_after_all_pages() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  Occupancy occupancy;
  As608OccupancyReader reader(sensor, occupancy);
  TEST_ASSERT_TRUE(reader.begin());
  TEST_ASSERT_TRUE(reader.active());
  TEST_ASSERT_FALSE(occupancy.valid);

  uint8_t p0[32] = {0};
  p0[0] = 0x01;       // physical page 0, slot 0 -> product ID 1 under current E18
  p0[31] = 0x80;      // physical page 0, slot 255 -> product ID 256
  link.ack(0, p0, sizeof p0);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, reader.step()); // starts page 0
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, reader.step()); // page 0 read, page 1 still pending
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_TRUE(occupancy.has(1));
  TEST_ASSERT_TRUE(occupancy.has(256));

  uint8_t p1[32] = {0};
  p1[0] = 0x01;       // physical page 1, slot 0 -> product ID 257
  p1[5] = 0x08;       // physical page 1, slot 43 -> product ID 300
  p1[6] = 0x01;       // slot 304, beyond capacity 300; ignored
  link.ack(0, p1, sizeof p1);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, reader.step()); // starts page 1
  TEST_ASSERT_EQUAL_UINT8(1, link.tx[10]);
  TEST_ASSERT_EQUAL_INT(EIO_OK, reader.step());      // only now is the full map valid
  TEST_ASSERT_FALSE(reader.active());
  TEST_ASSERT_TRUE(occupancy.valid);
  TEST_ASSERT_EQUAL_UINT32(4, occupancy.count());
  TEST_ASSERT_TRUE(occupancy.has(1) && occupancy.has(256));
  TEST_ASSERT_TRUE(occupancy.has(257) && occupancy.has(300));
  TEST_ASSERT_FALSE(occupancy.has(305));
}

void test_occupancy_reader_discards_partial_map_after_page_error() {
  FakeAs608Link link; As608EnrollSensor sensor(link, 0);
  authenticate(sensor, link); setCapacity(sensor, link, 300);
  Occupancy occupancy;
  As608OccupancyReader reader(sensor, occupancy);
  TEST_ASSERT_TRUE(reader.begin());
  uint8_t p0[32] = {0}; p0[0] = 0x01;
  link.ack(0, p0, sizeof p0);
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, reader.step());
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, reader.step());
  TEST_ASSERT_FALSE(occupancy.valid);
  link.ack(0x01); // second page fails; a partial map must not survive as trusted data
  TEST_ASSERT_EQUAL_INT(EIO_PENDING, reader.step());
  TEST_ASSERT_EQUAL_INT(EIO_RETRYABLE, reader.step());
  TEST_ASSERT_FALSE(reader.active());
  TEST_ASSERT_FALSE(occupancy.valid);
  TEST_ASSERT_EQUAL_UINT32(0, occupancy.count());
}



int main() {
  UNITY_BEGIN();
  RUN_TEST(test_authenticate_sends_verify_password_only);
  RUN_TEST(test_read_capacity_parses_system_parameters);
  RUN_TEST(test_capture_maps_no_finger_and_bad_image_contextually);
  RUN_TEST(test_convert_validates_buffer_and_maps_image_errors);
  RUN_TEST(test_search_uses_full_capacity_and_returns_physical_page);
  RUN_TEST(test_no_match_is_only_a_search_result);
  RUN_TEST(test_search_rejects_page_outside_reported_capacity);
  RUN_TEST(test_create_model_maps_fingerprint_mismatch);
  RUN_TEST(test_store_uses_physical_page_and_never_retries_non_ok);
  RUN_TEST(test_bad_response_checksum_is_retryable_not_no_match);
  RUN_TEST(test_command_polls_without_resending_packet);
  RUN_TEST(test_transport_timeout_and_poisoned_link_are_fatal);
  RUN_TEST(test_read_index_page_handles_32_byte_bitmap_and_rejects_bad_page);
  RUN_TEST(test_occupancy_reader_validates_only_after_all_pages);
  RUN_TEST(test_occupancy_reader_discards_partial_map_after_page_error);
  return UNITY_END();
}
