#include <unity.h>
#include <string.h>
#include "event_codec.h"
#include "time_rules.h"
#include "status_derive.h"
using namespace att;

void setUp() {}
void tearDown() {}

// ---------- event_codec ----------
void test_crc_vector() {
  const uint8_t s[] = {'1','2','3','4','5','6','7','8','9'};
  TEST_ASSERT_EQUAL_UINT16(0x29B1, crc16(s, 9));
}
void test_layout_bytes() {
  Event e = {EV_SCAN, makeFlags(1, ST_LATE, false, false, false), 0x1234, 0xA1B2C3D4u, 0x01020304u, 0x5678};
  uint8_t o[REC_SIZE];
  encode(e, o);
  uint8_t want[14] = {1, 0x05, 0x34, 0x12, 0xD4, 0xC3, 0xB2, 0xA1, 0x04, 0x03, 0x02, 0x01, 0x78, 0x56};
  TEST_ASSERT_TRUE(memcmp(o, want, 14) == 0);
  TEST_ASSERT_EQUAL_UINT16(crc16(o, 14), (uint16_t)(o[14] | (o[15] << 8)));
}
void test_roundtrip() {
  Event e = {EV_CLOSE, makeFlags(0, ST_CLEAR, true, true, false), 0xFFFF, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFF};
  uint8_t o[REC_SIZE];
  Event d = {0, 0, 0, 0, 0, 0};
  encode(e, o);
  TEST_ASSERT_TRUE(decode(o, d));
  TEST_ASSERT_EQUAL_UINT8(e.type, d.type);
  TEST_ASSERT_EQUAL_UINT8(e.flags, d.flags);
  TEST_ASSERT_EQUAL_UINT16(e.a, d.a);
  TEST_ASSERT_EQUAL_UINT32(e.epoch, d.epoch);
  TEST_ASSERT_EQUAL_UINT32(e.seq, d.seq);
  TEST_ASSERT_EQUAL_UINT16(e.b, d.b);
}
void test_reject_every_single_bit_flip() {
  Event e = {EV_MARK, makeFlags(1, ST_EXCUSED, false, false, true), 45, 1793318400u, 77, 2494};
  uint8_t o[REC_SIZE];
  Event d;
  encode(e, o);
  for (int bit = 0; bit < 128; bit++) {
    o[bit / 8] ^= (uint8_t)(1 << (bit % 8));
    TEST_ASSERT_FALSE(decode(o, d));
    o[bit / 8] ^= (uint8_t)(1 << (bit % 8));
  }
  TEST_ASSERT_TRUE(decode(o, d));
}
void test_reject_erased_and_bad_type() {
  uint8_t o[REC_SIZE];
  Event d;
  memset(o, 0xFF, sizeof o);
  TEST_ASSERT_FALSE(decode(o, d));
  memset(o, 0x00, sizeof o);
  TEST_ASSERT_FALSE(decode(o, d));
  Event z = {EV_NONE, 0, 1, 2, 3, 4};
  encode(z, o);
  TEST_ASSERT_FALSE(decode(o, d));
  Event hi = {(uint8_t)(EV_MAX + 1), 0, 1, 2, 3, 4};
  encode(hi, o);
  TEST_ASSERT_FALSE(decode(o, d));
}
void test_flags() {
  for (int s = 0; s < 2; s++)
    for (int st = 0; st < 4; st++)
      for (int k = 0; k < 8; k++) {
        uint8_t f = makeFlags((uint8_t)s, (uint8_t)st, k & 1, k & 2, k & 4);
        TEST_ASSERT_EQUAL_UINT8(s, flagSession(f));
        TEST_ASSERT_EQUAL_UINT8(st, flagStatus(f));
        TEST_ASSERT_EQUAL_INT((k & 1) != 0, flagCoverage(f));
        TEST_ASSERT_EQUAL_INT((k & 2) != 0, flagMore(f));
        TEST_ASSERT_EQUAL_INT((k & 4) != 0, flagWholeDay(f));
      }
}
static void bitmap_case(uint16_t cap) {
  uint64_t w[5] = {0, 0, 0, 0, 0};
  uint16_t ids[] = {1, 2, 63, 64, 65, 128, 129, cap};
  for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) bitmapSet(w, ids[i]);
  uint16_t n = payloadCount(cap);
  uint64_t w2[5] = {0, 0, 0, 0, 0};
  for (uint16_t i = 0; i < n; i++) {                 // through the record codec
    uint8_t o[REC_SIZE];
    Event d;
    encode(makePayload(w[i], 100 + i, i + 1 < n), o);
    TEST_ASSERT_TRUE(decode(o, d));
    TEST_ASSERT_EQUAL_INT(EV_PAYLOAD, d.type);
    TEST_ASSERT_EQUAL_INT(i + 1 < n, flagMore(d.flags));
    w2[i] = payloadBits(d);
  }
  for (uint16_t id = 1; id <= cap; id++) {
    bool want = false;
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) if (ids[i] == id) want = true;
    TEST_ASSERT_EQUAL_INT(want, bitmapTest(w2, id));
  }
}
void test_bitmap_162() { TEST_ASSERT_EQUAL_UINT16(3, payloadCount(162)); bitmap_case(162); }
void test_bitmap_300() { TEST_ASSERT_EQUAL_UINT16(5, payloadCount(300)); bitmap_case(300); }
void test_payload_counts() {
  TEST_ASSERT_EQUAL_UINT16(0, payloadCount(0));
  TEST_ASSERT_EQUAL_UINT16(1, payloadCount(1));
  TEST_ASSERT_EQUAL_UINT16(1, payloadCount(64));
  TEST_ASSERT_EQUAL_UINT16(2, payloadCount(65));
}
void test_payload_all_bits() {
  uint64_t v = 0xFEDCBA9876543210ull;
  TEST_ASSERT_TRUE(payloadBits(makePayload(v, 1, false)) == v);
  TEST_ASSERT_TRUE(payloadBits(makePayload(~v, 1, false)) == ~v);
}

// ---------- time_rules ----------
static const uint32_t FRI_2026_10_30 = 1793318400u;   // 00:00 UTC, day 2494
void test_max_term_day_is_last_full_local_day_for_all_supported_offsets() {
  const uint64_t minusTwelveEnd =
      (uint64_t)dayStartEpoch(MAX_TERM_DAY, -720) + 86399u;
  const uint64_t nextMinusTwelveEnd =
      (uint64_t)dayStartEpoch(MAX_TERM_DAY + 1, -720) + 86399u;
  const uint64_t plusFourteenEnd =
      (uint64_t)dayStartEpoch(MAX_TERM_DAY, 840) + 86399u;
  TEST_ASSERT_TRUE(minusTwelveEnd <= 0xFFFFFFFFull);
  TEST_ASSERT_TRUE(plusFourteenEnd <= 0xFFFFFFFFull);
  TEST_ASSERT_TRUE(nextMinusTwelveEnd > 0xFFFFFFFFull);
  TEST_ASSERT_EQUAL_INT(MAX_TERM_DAY,
      dayNumber((uint32_t)minusTwelveEnd, -720));
  TEST_ASSERT_EQUAL_INT(MAX_TERM_DAY + 1, dayNumber(0xFFFFFFFFu, -720));
}
void test_day_numbers() {
  TEST_ASSERT_EQUAL_INT(0, dayNumber(EPOCH_2020, 0));
  TEST_ASSERT_EQUAL_INT(-1, dayNumber(EPOCH_2020 - 1, 0));
  TEST_ASSERT_EQUAL_INT(0, dayNumber(EPOCH_2020 - 28800, 480));     // UTC+8 local midnight
  TEST_ASSERT_EQUAL_INT(-1, dayNumber(EPOCH_2020 - 28801, 480));
  TEST_ASSERT_EQUAL_INT(2494, dayNumber(FRI_2026_10_30, 0));
  TEST_ASSERT_EQUAL_INT(2494, dayNumber(FRI_2026_10_30 + 86399, 0));
  TEST_ASSERT_EQUAL_INT(2495, dayNumber(FRI_2026_10_30 + 86400, 0));
  TEST_ASSERT_EQUAL_INT(2494, dayNumber(FRI_2026_10_30 + 8 * 3600 - 1, 480));
  TEST_ASSERT_EQUAL_INT(2495, dayNumber(FRI_2026_10_30 + 16 * 3600, 480));   // 00:00 local next day
}
void test_weekdays() {
  TEST_ASSERT_EQUAL_UINT8(2, weekdayIndex(0));        // Wed
  TEST_ASSERT_EQUAL_UINT8(4, weekdayIndex(2494));     // Fri
  TEST_ASSERT_EQUAL_UINT8(5, weekdayIndex(2495));     // Sat
  TEST_ASSERT_EQUAL_UINT8(0, weekdayIndex(2497));     // Mon
}
void test_minute_of_day() {
  TEST_ASSERT_EQUAL_UINT16(0, minuteOfDay(FRI_2026_10_30, 0));
  TEST_ASSERT_EQUAL_UINT16(1439, minuteOfDay(FRI_2026_10_30 + 86399, 0));
  TEST_ASSERT_EQUAL_UINT16(480, minuteOfDay(FRI_2026_10_30, 480));
  TEST_ASSERT_EQUAL_UINT16(1380, minuteOfDay(FRI_2026_10_30, -60));
}
static uint32_t at(uint32_t dayStart, int minute, int sec = 0) { return dayStart + (uint32_t)minute * 60 + (uint32_t)sec; }
void test_session_windows() {
  Timetable t = defaultTimetable();
  Params p = {0, 10, 30};
  TEST_ASSERT_EQUAL_INT(-1, sessionAt(t, p, at(FRI_2026_10_30, 449, 59)));
  TEST_ASSERT_EQUAL_INT(0, sessionAt(t, p, at(FRI_2026_10_30, 450)));
  TEST_ASSERT_EQUAL_INT(0, sessionAt(t, p, at(FRI_2026_10_30, 749, 59)));
  TEST_ASSERT_EQUAL_INT(-1, sessionAt(t, p, at(FRI_2026_10_30, 750)));
  TEST_ASSERT_EQUAL_INT(-1, sessionAt(t, p, at(FRI_2026_10_30, 779, 59)));
  TEST_ASSERT_EQUAL_INT(1, sessionAt(t, p, at(FRI_2026_10_30, 780)));
  TEST_ASSERT_EQUAL_INT(1, sessionAt(t, p, at(FRI_2026_10_30, 989, 59)));
  TEST_ASSERT_EQUAL_INT(-1, sessionAt(t, p, at(FRI_2026_10_30, 990)));
}
void test_weekend_and_offset_and_disabled() {
  Timetable t = defaultTimetable();
  Params p = {0, 10, 30};
  TEST_ASSERT_EQUAL_INT(-1, sessionAt(t, p, at(FRI_2026_10_30 + 86400, 600)));       // Saturday
  Params p8 = {480, 10, 30};
  TEST_ASSERT_EQUAL_INT(0, sessionAt(t, p8, at(FRI_2026_10_30, 0)));   // 08:00 local = 00:00 UTC
  t.day[4][1].enabled = false;
  TEST_ASSERT_EQUAL_INT(-1, sessionAt(t, p, at(FRI_2026_10_30, 900)));               // Friday PM disabled
  TEST_ASSERT_EQUAL_INT(1, sessionAt(t, p, at(FRI_2026_10_30 - 86400, 900)));        // Thursday PM still on
}
void test_late_boundary() {
  Slot s = {480, 750, true};
  TEST_ASSERT_FALSE(isLate(s, 10, 490));
  TEST_ASSERT_TRUE(isLate(s, 10, 491));
  TEST_ASSERT_FALSE(isLate(s, 10, 465));
  TEST_ASSERT_FALSE(isLate(s, 0, 480));
  TEST_ASSERT_TRUE(isLate(s, 0, 481));
  TEST_ASSERT_TRUE(hasStarted(s, 480));
  TEST_ASSERT_FALSE(hasStarted(s, 479));
  TEST_ASSERT_TRUE(hasEnded(s, 750));
  TEST_ASSERT_FALSE(hasEnded(s, 749));
}
void test_timetable_validation() {
  Timetable t = defaultTimetable();
  TEST_ASSERT_TRUE(validateTimetable(t, 15));
  t.day[2][0].end = t.day[2][0].start;                       // empty session
  TEST_ASSERT_FALSE(validateTimetable(t, 15));
  t = defaultTimetable();
  t.day[1][1].start = 764;                                   // 764-15 = 749 < 750
  TEST_ASSERT_FALSE(validateTimetable(t, 15));
  t.day[1][1].start = 765;                                   // exactly touching: ok
  TEST_ASSERT_TRUE(validateTimetable(t, 15));
  t = defaultTimetable();
  t.day[0][1].end = 1440;
  TEST_ASSERT_FALSE(validateTimetable(t, 15));
  t = defaultTimetable();
  t.day[3][0].start = 10;                                    // window would start before midnight
  TEST_ASSERT_FALSE(validateTimetable(t, 15));
  t = defaultTimetable();
  t.day[3][0].enabled = false;
  t.day[3][0].start = 5; t.day[3][0].end = 1;                // garbage but disabled: ignored
  TEST_ASSERT_TRUE(validateTimetable(t, 15));
}

// ---------- status_derive ----------
static Facts F(bool nc, bool ex, bool st, bool cl, bool cov, uint8_t mk, uint8_t sc) {
  Facts f = {nc, ex, st, cl, cov, mk, sc};
  return f;
}
void test_derive_precedence() {
  TEST_ASSERT_EQUAL_INT(D_NOCLASS, derive(F(1, 1, 1, 1, 1, 1, 1)));       // no-class beats everything
  TEST_ASSERT_EQUAL_INT(D_EXCUSED, derive(F(0, 1, 1, 1, 1, 3, 1)));       // mark beats scan
  TEST_ASSERT_EQUAL_INT(D_LATE, derive(F(0, 1, 1, 1, 1, 2, 1)));
  TEST_ASSERT_EQUAL_INT(D_PRESENT, derive(F(0, 1, 1, 1, 1, 1, 2)));
  TEST_ASSERT_EQUAL_INT(D_LATE, derive(F(0, 1, 1, 0, 0, 0, 2)));          // CLEAR/none falls to scan
  TEST_ASSERT_EQUAL_INT(D_PRESENT, derive(F(0, 1, 1, 1, 0, 0, 1)));       // scan wins even without coverage
}
void test_derive_unrecorded() {
  TEST_ASSERT_EQUAL_INT(D_NA, derive(F(0, 0, 1, 1, 1, 0, 0)));
  TEST_ASSERT_EQUAL_INT(D_UPCOMING, derive(F(0, 1, 0, 0, 0, 0, 0)));
  TEST_ASSERT_EQUAL_INT(D_PENDING, derive(F(0, 1, 1, 0, 0, 0, 0)));
  TEST_ASSERT_EQUAL_INT(D_ABSENT, derive(F(0, 1, 1, 1, 1, 0, 0)));
  TEST_ASSERT_EQUAL_INT(D_UNVERIFIED, derive(F(0, 1, 1, 1, 0, 0, 0)));
}
void test_derive_mark_for_unexpected_student() {
  TEST_ASSERT_EQUAL_INT(D_PRESENT, derive(F(0, 0, 1, 1, 1, 1, 0)));       // explicit mark still shown
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc_vector);
  RUN_TEST(test_layout_bytes);
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_reject_every_single_bit_flip);
  RUN_TEST(test_reject_erased_and_bad_type);
  RUN_TEST(test_flags);
  RUN_TEST(test_bitmap_162);
  RUN_TEST(test_bitmap_300);
  RUN_TEST(test_payload_counts);
  RUN_TEST(test_payload_all_bits);
  RUN_TEST(test_max_term_day_is_last_full_local_day_for_all_supported_offsets);
  RUN_TEST(test_day_numbers);
  RUN_TEST(test_weekdays);
  RUN_TEST(test_minute_of_day);
  RUN_TEST(test_session_windows);
  RUN_TEST(test_weekend_and_offset_and_disabled);
  RUN_TEST(test_late_boundary);
  RUN_TEST(test_timetable_validation);
  RUN_TEST(test_derive_precedence);
  RUN_TEST(test_derive_unrecorded);
  RUN_TEST(test_derive_mark_for_unexpected_student);
  return UNITY_END();
}
