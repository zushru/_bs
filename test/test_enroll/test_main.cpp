#include <limits.h>
#include <unity.h>
#include <string.h>
#include <vector>
#include "enrolled_roster.h"
#include "enroll_fsm.h"
using namespace att;

void setUp() {}
void tearDown() {}

struct MemStore : IStorage {
  std::vector<uint8_t> d;
  bool sizeChecked(size_t& out) override { out = d.size(); return true; }
  bool read(size_t o, uint8_t* b, size_t n) override { if (o + n > d.size()) return false; memcpy(b, d.data() + o, n); return true; }
  bool append(const uint8_t* b, size_t n) override { d.insert(d.end(), b, b + n); return true; }
  bool truncate(size_t n) override { d.resize(n); return true; }
};
static const int32_t FRI = 2494;
static uint32_t E(int32_t day, int minute) { return dayStartEpoch(day, 0) + (uint32_t)minute * 60; }
struct Rig {
  MemStore ms; LogStore ls; RosterStore r;
  Rig() : ls(ms), r(0) { ls.open(); ls.attach(&r); }
  void add(uint16_t id, const char* nm, int32_t day = FRI - 10) {
    Event e[5]; size_t c;
    r.planSet(id, nm, strlen(nm), E(day, 600), e, c); ls.append(e, c);
  }
  void off(uint16_t id, int32_t day) {
    Event e[5]; size_t c;
    r.planDeactivate(id, E(day, 600), e, c); ls.append(e, c);
  }
};

void test_id_page_mapping_boundaries() {
  TEST_ASSERT_EQUAL_UINT16(0, idToPage(1));
  TEST_ASSERT_EQUAL_UINT16(299, idToPage(300));        // the last valid page of a 300-template sensor
  for (uint16_t id = 1; id <= 300; id++) TEST_ASSERT_EQUAL_UINT16(id, pageToId(idToPage(id)));
}
void test_occupancy_basics_and_bounds() {
  Occupancy o;
  TEST_ASSERT_FALSE(o.valid);
  o.setPresent(1, true); o.setPresent(300, true); o.setPresent(45, true);
  o.setPresent(0, true); o.setPresent(301, true); o.setPresent(65535, true);
  TEST_ASSERT_EQUAL_UINT32(3, o.count());
  TEST_ASSERT_TRUE(o.has(1) && o.has(300) && o.has(45));
  TEST_ASSERT_FALSE(o.has(0)); TEST_ASSERT_FALSE(o.has(301)); TEST_ASSERT_FALSE(o.has(2));
  o.setPresent(45, false);
  TEST_ASSERT_FALSE(o.has(45));
  o.begin();
  TEST_ASSERT_EQUAL_UINT32(0, o.count());
  TEST_ASSERT_FALSE(o.valid);
}
void test_index_page_decoding() {
  Occupancy o; uint8_t p0[32] = {0}, p1[32] = {0};
  p0[0] = 0x01;            // slot 0   -> ID 1
  p0[1] = 0x80;            // slot 15  -> ID 16
  p0[31] = 0x80;           // slot 255 -> ID 256
  p1[0] = 0x01;            // slot 256 -> ID 257
  p1[5] = 0x08;            // slot 256+43 = 299 -> ID 300
  p1[6] = 0x01;            // slot 304: beyond capacity 300, ignored
  o.begin();
  o.addIndexPage(0, p0, 300);
  TEST_ASSERT_FALSE(o.valid);                           // pages added but read not finished
  o.addIndexPage(1, p1, 300);
  o.finish();
  TEST_ASSERT_TRUE(o.valid);
  TEST_ASSERT_EQUAL_UINT32(5, o.count());
  TEST_ASSERT_TRUE(o.has(1) && o.has(16) && o.has(256) && o.has(257) && o.has(300));
  Occupancy small; small.begin(); small.addIndexPage(0, p0, 100); small.finish();   // capacity 100
  TEST_ASSERT_EQUAL_UINT32(2, small.count());
  uint8_t junk[32]; memset(junk, 0xFF, 32);
  Occupancy x; x.addIndexPage(4, junk, 300);            // no such page
  TEST_ASSERT_EQUAL_UINT32(0, x.count());
  TEST_ASSERT_EQUAL_UINT8(2, Occupancy::pagesNeeded(300));
  TEST_ASSERT_EQUAL_UINT8(1, Occupancy::pagesNeeded(256));
  TEST_ASSERT_EQUAL_UINT8(2, Occupancy::pagesNeeded(257));
}
void test_expected_requires_template_and_valid_read() {
  Rig g; g.add(1, "Ana"); g.add(2, "Ben"); g.add(3, "Cy");
  Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(3, true);
  EnrolledRoster er(g.r, o);
  TEST_ASSERT_FALSE(er.expected(1, FRI));               // occupancy not finished: nobody
  o.finish();
  TEST_ASSERT_TRUE(er.expected(1, FRI));
  TEST_ASSERT_FALSE(er.expected(2, FRI));               // no template
  TEST_ASSERT_TRUE(er.expected(3, FRI));
  TEST_ASSERT_FALSE(er.expected(1, FRI - 20));          // before the student's start day
  g.off(3, FRI - 2);
  TEST_ASSERT_FALSE(er.expected(3, FRI));               // deactivated: template alone is not enough
  o.setPresent(9, true);
  TEST_ASSERT_FALSE(er.expected(9, FRI));               // a stray template is not a student
  o.begin();                                            // refresh restarted: back to nobody
  TEST_ASSERT_FALSE(er.expected(1, FRI));
}
void test_reconcile_counts() {
  Rig g; g.add(1, "Ana"); g.add(2, "Ben"); g.add(3, "Cy"); g.add(4, "Di"); g.off(4, FRI - 5);
  Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(3, true); o.setPresent(4, true); o.setPresent(7, true);
  Reconcile bad = reconcile(g.r, o);
  TEST_ASSERT_FALSE(bad.valid);
  TEST_ASSERT_EQUAL_UINT16(0, bad.matched);
  o.finish();
  Reconcile x = reconcile(g.r, o);
  TEST_ASSERT_TRUE(x.valid);
  TEST_ASSERT_EQUAL_UINT16(2, x.matched);               // 1, 3
  TEST_ASSERT_EQUAL_UINT16(1, x.orphans);               // 2
  TEST_ASSERT_EQUAL_UINT16(2, x.firstOrphan);
  TEST_ASSERT_EQUAL_UINT16(1, x.strays);                // 7 (4 is a known, inactive student: not a stray)
  TEST_ASSERT_EQUAL_UINT16(7, x.firstStray);
}
void test_queue_and_slot_allocation_helpers_refuse_invalid_occupancy() {
  Rig g; g.add(1, "A");
  Occupancy invalid; invalid.begin();
  TEST_ASSERT_EQUAL_UINT16(0, nextToEnroll(g.r, invalid, 0));
  TEST_ASSERT_EQUAL_UINT16(0, firstFreeSlot(g.r, invalid, 300));
  TEST_ASSERT_FALSE(reconcile(g.r, invalid).valid);
}

void test_queue_and_slot_allocation() {
  Rig g; g.add(1, "A1"); g.add(2, "B2"); g.add(3, "C3"); g.add(5, "E5"); g.off(3, FRI - 5);
  Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(4, true); o.finish();
  TEST_ASSERT_EQUAL_UINT16(2, nextToEnroll(g.r, o, 0));
  TEST_ASSERT_EQUAL_UINT16(5, nextToEnroll(g.r, o, 2));  // 3 is inactive, 4 is not a student
  TEST_ASSERT_EQUAL_UINT16(0, nextToEnroll(g.r, o, 5));
  TEST_ASSERT_EQUAL_UINT16(0, nextToEnroll(g.r, o, 65535));   // no wraparound
  TEST_ASSERT_EQUAL_UINT16(6, firstFreeSlot(g.r, o, 300));    // 1,2,3(inactive),5 known; 4 occupied
  TEST_ASSERT_EQUAL_UINT16(0, firstFreeSlot(g.r, o, 5));      // capacity 5: all taken
  Rig empty; Occupancy e; e.begin(); e.finish();
  TEST_ASSERT_EQUAL_UINT16(1, firstFreeSlot(empty.r, e, 300));
  TEST_ASSERT_EQUAL_UINT16(0, firstFreeSlot(empty.r, e, 0));
}
// ---- end to end: a restore onto a blank sensor must not produce false Absent marks ----
static int closedExpected(Occupancy& o, Rig& g, int* closes) {
  Params p = {0, 30, 15};
  Timetable tt = defaultTimetable();
  EnrolledRoster er(g.r, o);
  ClosureEngine eng(g.ls, p, tt, er, 300, FRI);
  eng.onBoot(E(FRI, 400), 0);
  for (uint32_t t = E(FRI, 465); t <= E(FRI, 760); t += 10) { eng.onHealth(t, true, GAP_SENSOR); eng.onTick(t); eng.step(t); }
  Event e; int exp = -1; *closes = 0;
  for (size_t i = 0; i < g.ls.count(); i++) {
    g.ls.at(i, e);
    if (e.type == EV_CLOSE && flagSession(e.flags) == 0 && e.b == FRI) { exp = e.a; (*closes)++; }
  }
  return exp;
}
void test_close_snapshot_excludes_unenrolled() {
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");
    Occupancy o; o.begin(); for (uint16_t id = 1; id <= 5; id++) o.setPresent(id, true); o.finish();
    int closes; TEST_ASSERT_EQUAL_INT(5, closedExpected(o, g, &closes)); TEST_ASSERT_EQUAL_INT(1, closes); }
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");            // restored roster, sensor has 3 templates
    Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(2, true); o.setPresent(5, true); o.finish();
    int closes; TEST_ASSERT_EQUAL_INT(3, closedExpected(o, g, &closes)); }
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");            // blank replacement sensor
    Occupancy o; o.begin(); o.finish();
    int closes; TEST_ASSERT_EQUAL_INT(0, closedExpected(o, g, &closes)); }
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");            // index read never finished
    Occupancy o; o.begin(); for (uint16_t id = 1; id <= 5; id++) o.setPresent(id, true);
    int closes; TEST_ASSERT_EQUAL_INT(0, closedExpected(o, g, &closes)); }
}


struct FakeEnrollSensor : IEnrollSensor {
  std::vector<EnrollIo> captures, converts, searches, creates, stores;
  std::vector<uint16_t> pages;
  size_t capturePos = 0, convertPos = 0, searchPos = 0, createPos = 0, storePos = 0, pagePos = 0;
  uint16_t score = 321, lastStoredPage = 0xFFFF;
  uint8_t lastBuffer = 0;
  int captureCalls = 0, convertCalls = 0, searchCalls = 0, createCalls = 0, storeCalls = 0;
  bool pending = false;
  bool operationPending() const override { return pending; }
  static EnrollIo next(const std::vector<EnrollIo>& values, size_t& pos, EnrollIo fallback) {
    return pos < values.size() ? values[pos++] : fallback;
  }
  EnrollIo captureImage() override {
    ++captureCalls;
    EnrollIo result = next(captures, capturePos, (captureCalls % 2) == 0 ? EIO_NO_FINGER : EIO_OK);
    pending = result == EIO_PENDING;
    return result;
  }
  EnrollIo convertImage(uint8_t buffer) override {
    ++convertCalls; lastBuffer = buffer;
    EnrollIo result = next(converts, convertPos, EIO_OK);
    pending = result == EIO_PENDING;
    return result;
  }
  EnrollIo searchTemplate(uint16_t& page, uint16_t& outScore) override {
    ++searchCalls;
    page = pagePos < pages.size() ? pages[pagePos++] : 0;
    outScore = score;
    EnrollIo result = next(searches, searchPos, EIO_NO_MATCH);
    pending = result == EIO_PENDING;
    return result;
  }
  EnrollIo createModel() override {
    ++createCalls;
    EnrollIo result = next(creates, createPos, EIO_OK);
    pending = result == EIO_PENDING;
    return result;
  }
  EnrollIo storeModel(uint16_t page) override {
    ++storeCalls; lastStoredPage = page;
    EnrollIo result = next(stores, storePos, EIO_OK);
    pending = result == EIO_PENDING;
    return result;
  }
};

static void validEmptyOccupancy(Occupancy& o) { o.begin(); o.finish(); }
static void driveToStore(EnrollFsm& f) {
  for (int i = 0; i < 7; i++) f.step();
}

void test_fsm_begin_requires_valid_occupancy() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A");
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OCCUPANCY_INVALID, f.begin(300, false));
  TEST_ASSERT_EQUAL_INT(EN_IDLE, f.state());
}
void test_fsm_queue_is_active_roster_minus_occupancy() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); g.add(2, "B"); g.add(3, "C");
  g.off(3, FRI - 5);
  o.begin(); o.setPresent(1, true); o.finish();
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  TEST_ASSERT_EQUAL_UINT16(1, f.queueSize());
  TEST_ASSERT_EQUAL_UINT16(1, f.queuePosition());
  TEST_ASSERT_EQUAL_UINT16(2, f.currentId());
}
void test_fsm_rejects_active_student_above_sensor_capacity() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(300, "Last"); validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_ID_OUT_OF_RANGE, f.begin(299, false));
}
void test_fsm_enforces_45_active_student_limit() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  for (uint16_t id = 1; id <= 46; id++) g.add(id, "S");
  validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_STUDENT_LIMIT, f.begin(300, false));
}
void test_fsm_empty_queue_finishes_immediately() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); o.begin(); o.setPresent(1, true); o.finish();
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_NO_WORK, f.begin(300, true));
  TEST_ASSERT_EQUAL_INT(EN_DONE, f.state());
  TEST_ASSERT_EQUAL_UINT16(0, f.queueSize());
}
void test_fsm_duplicate_blocks_store_and_reports_existing_id() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.searches.push_back(EIO_OK); sensor.pages.push_back(9);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step(); f.step(); f.step();
  TEST_ASSERT_EQUAL_INT(EN_DUPLICATE_FOUND, f.state());
  TEST_ASSERT_EQUAL_UINT16(10, f.duplicateId());
  TEST_ASSERT_EQUAL_UINT16(321, f.duplicateScore());
  TEST_ASSERT_EQUAL_INT(0, sensor.storeCalls);
  TEST_ASSERT_TRUE(f.tryDifferentFinger());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
}
void test_fsm_can_skip_duplicate_and_continue_queue() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); g.add(2, "B"); validEmptyOccupancy(o);
  sensor.searches.push_back(EIO_OK); sensor.pages.push_back(4);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step(); f.step(); f.step();
  TEST_ASSERT_TRUE(f.skipCurrent());
  TEST_ASSERT_EQUAL_UINT16(1, f.skippedCount());
  TEST_ASSERT_EQUAL_UINT16(2, f.currentId());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
}
void test_fsm_stores_at_id_minus_one_and_advances_after_release() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); g.add(2, "B"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK, EIO_NO_FINGER, EIO_OK, EIO_OK, EIO_NO_FINGER, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  TEST_ASSERT_EQUAL_INT(EN_STORE_MODEL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(1, sensor.storeCalls);
  TEST_ASSERT_EQUAL_UINT16(0, sensor.lastStoredPage);
  TEST_ASSERT_TRUE(o.has(1));
  TEST_ASSERT_EQUAL_UINT16(1, f.enrolledCount());
  TEST_ASSERT_EQUAL_UINT16(2, f.currentId());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();  // Previous finger remains placed; do not accept it for student 2.
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  TEST_ASSERT_EQUAL_INT(2, sensor.convertCalls);  // Only the first student's pair was converted.
  f.step();  // Finger is now removed.
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
}

void test_fsm_waits_for_first_finger_removal_before_second_sample() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK, EIO_OK, EIO_NO_FINGER, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step(); f.step(); f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();  // Same finger is still present.
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  TEST_ASSERT_EQUAL_INT(1, sensor.convertCalls);
  f.step();  // Explicit no-finger result unlocks the second sample.
  TEST_ASSERT_EQUAL_INT(EN_WAIT_SECOND_FINGER, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_CONVERT_SECOND, f.state());
}
void test_fsm_verify_toggle_off_skips_post_store_scan() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f); f.step();
  TEST_ASSERT_FALSE(f.verificationEnabled());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  TEST_ASSERT_EQUAL_UINT16(0, f.currentId());
  TEST_ASSERT_FALSE(f.skipCurrent());  // There is no next student to skip while waiting for release.
  f.step();  // End the batch only after the enrollment finger is gone.
  TEST_ASSERT_EQUAL_INT(EN_DONE, f.state());
  TEST_ASSERT_EQUAL_INT(1, sensor.searchCalls);  // duplicate check only
  TEST_ASSERT_EQUAL_INT(4, sensor.captureCalls); // includes final finger-release check
  TEST_ASSERT_EQUAL_UINT16(0, f.verifiedCount());
}
void test_fsm_verify_toggle_on_requires_remove_then_matching_scan() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK, EIO_NO_FINGER, EIO_OK, EIO_NO_FINGER, EIO_OK};
  sensor.searches = {EIO_NO_MATCH, EIO_OK};
  sensor.pages = {0, 0};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, true));
  driveToStore(f); f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_WAIT_VERIFY_FINGER, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_CONVERT_VERIFY, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_SEARCH_VERIFY, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_DONE, f.state());
  TEST_ASSERT_EQUAL_UINT16(1, f.enrolledCount());
  TEST_ASSERT_EQUAL_UINT16(1, f.verifiedCount());
}
void test_fsm_verify_failure_is_visible_and_can_continue_without_deleting() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK, EIO_NO_FINGER, EIO_OK, EIO_NO_FINGER, EIO_OK};
  sensor.searches = {EIO_NO_MATCH, EIO_NO_MATCH};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, true));
  driveToStore(f); f.step(); f.step(); f.step(); f.step(); f.step();
  TEST_ASSERT_EQUAL_INT(EN_VERIFY_FAILED, f.state());
  TEST_ASSERT_TRUE(o.has(1));  // template remains stored; no unverified delete command is issued
  TEST_ASSERT_EQUAL_UINT16(1, f.enrolledCount());
  TEST_ASSERT_EQUAL_UINT16(0, f.duplicateId());
  TEST_ASSERT_TRUE(f.continueAfterVerificationFailure());
  TEST_ASSERT_EQUAL_UINT16(1, f.verificationFailedCount());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_DONE, f.state());
}
void test_fsm_no_finger_is_wait_not_an_error() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_NO_FINGER, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  TEST_ASSERT_FALSE(f.retry());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
}
void test_fsm_pauses_after_three_transient_errors_and_can_retry() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_RETRYABLE, EIO_RETRYABLE, EIO_RETRYABLE, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step(); f.step(); f.step();
  TEST_ASSERT_EQUAL_INT(EN_RETRY_REQUIRED, f.state());
  TEST_ASSERT_TRUE(f.retry());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
}
void test_fsm_model_mismatch_restarts_first_finger_pair() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.creates.push_back(EIO_MISMATCH);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  for (int i = 0; i < 6; i++) f.step();
  TEST_ASSERT_EQUAL_INT(EN_CREATE_MODEL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  TEST_ASSERT_EQUAL_INT(0, sensor.storeCalls);
}
void test_fsm_rechecks_occupancy_before_store() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  o.setPresent(1, true);  // an out-of-band write took this slot after the queue was built
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_SLOT_TAKEN, f.state());
  TEST_ASSERT_EQUAL_INT(0, sensor.storeCalls);
}
void test_fsm_begin_rejects_incomplete_roster_history() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  g.r.onRecord(Event{EV_ROSTER, FL_MORE, 1, E(FRI, 600), 0, RA_SET});
  g.r.onRecord(makePayload(0x0000000000000041ull, 0, true));
  TEST_ASSERT_EQUAL_UINT32(0, g.r.malformed());
  TEST_ASSERT_FALSE(g.r.historyComplete());

  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_ROSTER_UNTRUSTED, f.begin(300, false));
  TEST_ASSERT_EQUAL_UINT16(0, f.queueSize());
  TEST_ASSERT_EQUAL_UINT16(0, sensor.captureCalls);
  TEST_ASSERT_EQUAL_UINT16(0, sensor.storeCalls);
}

void test_fsm_rechecks_roster_history_before_store() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  TEST_ASSERT_EQUAL_INT(EN_STORE_MODEL, f.state());

  Event malformed = {EV_ROSTER, 0, 1, E(FRI, 601), 0, 99};
  TEST_ASSERT_TRUE(g.ls.append(&malformed, 1));
  TEST_ASSERT_FALSE(g.r.historyComplete());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_ROSTER_CHANGED, f.state());
  TEST_ASSERT_EQUAL_UINT16(0, sensor.storeCalls);
}

void test_fsm_rechecks_roster_before_store() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  g.off(1, FRI - 1);
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_ROSTER_CHANGED, f.state());
  TEST_ASSERT_EQUAL_INT(0, sensor.storeCalls);
}
void test_fsm_invalid_occupancy_before_store_blocks_write() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  o.begin();
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_OCCUPANCY_REFRESH_REQUIRED, f.state());
  TEST_ASSERT_EQUAL_INT(0, sensor.storeCalls);
}
void test_fsm_ambiguous_store_invalidates_and_refresh_detects_success() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK, EIO_NO_FINGER, EIO_OK, EIO_OK, EIO_NO_FINGER};
  sensor.stores.push_back(EIO_RETRYABLE);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f); f.step();
  TEST_ASSERT_EQUAL_INT(EN_OCCUPANCY_REFRESH_REQUIRED, f.state());
  TEST_ASSERT_FALSE(o.valid);
  f.step();
  TEST_ASSERT_EQUAL_INT(1, sensor.storeCalls);  // never blindly retry an uncertain store
  o.begin(); o.setPresent(1, true); o.finish();
  TEST_ASSERT_TRUE(f.resumeAfterOccupancyRefresh());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();  // Fake sensor still reports the finger held.
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_DONE, f.state());
  TEST_ASSERT_EQUAL_UINT16(1, f.enrolledCount());
}
void test_fsm_occupancy_refresh_cannot_resume_with_incomplete_roster_history() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.stores.push_back(EIO_RETRYABLE);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_OCCUPANCY_REFRESH_REQUIRED, f.state());

  Event malformed = {EV_ROSTER, 0, 1, E(FRI, 602), 0, 99};
  TEST_ASSERT_TRUE(g.ls.append(&malformed, 1));
  TEST_ASSERT_FALSE(g.r.historyComplete());
  o.begin(); o.finish();
  TEST_ASSERT_FALSE(f.resumeAfterOccupancyRefresh());
  TEST_ASSERT_EQUAL_INT(EN_OCCUPANCY_REFRESH_REQUIRED, f.state());
  TEST_ASSERT_EQUAL_UINT16(1, sensor.storeCalls);  // never blindly replay the ambiguous sensor write
}

void test_fsm_ambiguous_store_refresh_without_target_requeues_student() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK, EIO_NO_FINGER, EIO_OK, EIO_OK, EIO_NO_FINGER};
  sensor.stores.push_back(EIO_FATAL);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f); f.step();
  o.begin(); o.finish();  // full index refresh confirms the target page is still empty
  TEST_ASSERT_TRUE(f.resumeAfterOccupancyRefresh());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();  // Still held.
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  TEST_ASSERT_EQUAL_UINT16(1, f.currentId());
  TEST_ASSERT_EQUAL_UINT16(0, f.enrolledCount());
}

void test_fsm_skip_is_blocked_while_sensor_command_is_pending() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_OK};
  sensor.converts = {EIO_PENDING, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step();  // capture acknowledged
  TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
  f.step();  // convert has started but is still in flight
  TEST_ASSERT_TRUE(sensor.operationPending());
  TEST_ASSERT_FALSE(f.skipCurrent());
  TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
  f.step();  // drain the convert response before allowing the queue to advance
  TEST_ASSERT_FALSE(sensor.operationPending());
  TEST_ASSERT_EQUAL_INT(EN_SEARCH_DUPLICATE, f.state());
  TEST_ASSERT_TRUE(f.skipCurrent());
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
}

void test_fsm_cancel_waits_for_confirmed_finger_removal() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_PENDING, EIO_OK, EIO_PENDING, EIO_NO_FINGER};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.cancel();
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_BUSY, f.begin(300, false));
  f.step();  // Command has been sent; the result is not ready yet.
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  TEST_ASSERT_TRUE(f.sensorReserved());
  TEST_ASSERT_FALSE(f.retryCancelRelease());
  f.step();  // Finger is still held.
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  TEST_ASSERT_TRUE(f.sensorReserved());
  f.step();  // Release probe is still in flight: pending is not a sensor fault.
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  TEST_ASSERT_TRUE(f.sensorReserved());
  f.step();  // Only a positive no-finger result completes cancellation.
  TEST_ASSERT_EQUAL_INT(EN_CANCELLED, f.state());
  TEST_ASSERT_EQUAL_INT(4, sensor.captureCalls);
}


void test_fsm_cancel_drains_inflight_command_before_release_fence() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_PENDING, EIO_OK, EIO_NO_FINGER};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step();
  TEST_ASSERT_TRUE(sensor.operationPending());
  f.cancel();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  f.step();  // Finish the in-flight image command; do not issue a second command yet.
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  f.step();  // Start and complete the explicit no-finger check.
  TEST_ASSERT_EQUAL_INT(EN_CANCELLED, f.state());
  TEST_ASSERT_FALSE(f.sensorReserved());
}

void test_fsm_cancel_sensor_fault_keeps_ownership_until_retry_and_release() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_RETRYABLE, EIO_RETRYABLE, EIO_RETRYABLE, EIO_NO_FINGER};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.cancel();
  f.step(); f.step(); f.step();
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_RELEASE_FAULT, f.state());
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_BUSY, f.begin(300, false));
  const int pausedCalls = sensor.captureCalls;
  f.step();
  TEST_ASSERT_EQUAL_INT(pausedCalls, sensor.captureCalls);
  TEST_ASSERT_TRUE(f.retryCancelRelease());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_CANCELLED, f.state());
}



void test_fsm_pending_sensor_results_do_not_advance_or_consume_retries() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.captures = {EIO_PENDING, EIO_OK, EIO_PENDING, EIO_NO_FINGER};
  sensor.converts = {EIO_PENDING, EIO_OK};
  sensor.searches = {EIO_PENDING, EIO_NO_MATCH};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  f.step(); TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_CONVERT_FIRST, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_SEARCH_DUPLICATE, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_SEARCH_DUPLICATE, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  f.step(); TEST_ASSERT_EQUAL_INT(EN_WAIT_SECOND_FINGER, f.state());
}


void test_fsm_cancel_during_pending_store_applies_positive_ack() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.stores = {EIO_PENDING, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  f.step();
  TEST_ASSERT_TRUE(sensor.operationPending());
  TEST_ASSERT_EQUAL_INT(EN_STORE_MODEL, f.state());
  f.cancel();
  f.step();  // Drain the original store; do not issue another store or release probe yet.
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  TEST_ASSERT_TRUE(o.valid);
  TEST_ASSERT_TRUE(o.has(1));
  TEST_ASSERT_EQUAL_UINT16(1, f.enrolledCount());
  TEST_ASSERT_EQUAL_INT(2, sensor.storeCalls);  // initial send + one poll; no second store command
  TEST_ASSERT_TRUE(f.sensorReserved());
  f.step();  // Finger is now confirmed removed by the fake sensor.
  TEST_ASSERT_EQUAL_INT(EN_CANCELLED, f.state());
}

void test_fsm_cancel_during_ambiguous_store_invalidates_occupancy() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.stores = {EIO_PENDING, EIO_RETRYABLE};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  f.step();
  TEST_ASSERT_TRUE(sensor.operationPending());
  f.cancel();
  f.step();  // Lost/failed store result is ambiguous; fail closed before release.
  TEST_ASSERT_EQUAL_INT(EN_CANCEL_PENDING_RELEASE, f.state());
  TEST_ASSERT_FALSE(o.valid);
  TEST_ASSERT_TRUE(f.sensorReserved());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_CANCELLED, f.state());
  TEST_ASSERT_EQUAL_INT(2, sensor.storeCalls);  // ambiguous store is never blindly repeated
  TEST_ASSERT_FALSE(o.valid);  // Caller must refresh before attendance eligibility resumes.
}

void test_fsm_pending_store_does_not_invalidate_occupancy_or_repeat_write() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); validEmptyOccupancy(o);
  sensor.stores = {EIO_PENDING, EIO_OK};
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  driveToStore(f);
  TEST_ASSERT_EQUAL_INT(EN_STORE_MODEL, f.state());
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_STORE_MODEL, f.state());
  TEST_ASSERT_TRUE(o.valid);
  TEST_ASSERT_FALSE(o.has(1));
  TEST_ASSERT_EQUAL_INT(1, sensor.storeCalls);
  f.step();
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FINGER_REMOVAL, f.state());
  TEST_ASSERT_TRUE(o.has(1));
  TEST_ASSERT_EQUAL_INT(2, sensor.storeCalls);
}

void test_fsm_occupancy_refresh_does_not_requeue_prior_skips() {
  Rig g; FakeEnrollSensor sensor; Occupancy o;
  g.add(1, "A"); g.add(2, "B"); validEmptyOccupancy(o);
  sensor.stores.push_back(EIO_RETRYABLE);
  EnrollFsm f(g.r, o, sensor);
  TEST_ASSERT_EQUAL_INT(ES_BEGIN_OK, f.begin(300, false));
  TEST_ASSERT_TRUE(f.skipCurrent());
  TEST_ASSERT_EQUAL_UINT16(2, f.currentId());
  sensor.captures = {EIO_NO_FINGER, EIO_OK, EIO_NO_FINGER, EIO_OK};
  f.step();  // release any finger before starting the next queue entry
  TEST_ASSERT_EQUAL_INT(EN_WAIT_FIRST_FINGER, f.state());
  driveToStore(f); f.step();
  TEST_ASSERT_EQUAL_INT(EN_OCCUPANCY_REFRESH_REQUIRED, f.state());
  o.begin(); o.finish();
  TEST_ASSERT_TRUE(f.resumeAfterOccupancyRefresh());
  TEST_ASSERT_EQUAL_UINT16(1, f.queueSize());
  TEST_ASSERT_EQUAL_UINT16(2, f.currentId());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_id_page_mapping_boundaries);
  RUN_TEST(test_occupancy_basics_and_bounds);
  RUN_TEST(test_index_page_decoding);
  RUN_TEST(test_expected_requires_template_and_valid_read);
  RUN_TEST(test_reconcile_counts);
  RUN_TEST(test_queue_and_slot_allocation_helpers_refuse_invalid_occupancy);
  RUN_TEST(test_queue_and_slot_allocation);
  RUN_TEST(test_close_snapshot_excludes_unenrolled);
  RUN_TEST(test_fsm_begin_requires_valid_occupancy);
  RUN_TEST(test_fsm_queue_is_active_roster_minus_occupancy);
  RUN_TEST(test_fsm_rejects_active_student_above_sensor_capacity);
  RUN_TEST(test_fsm_enforces_45_active_student_limit);
  RUN_TEST(test_fsm_empty_queue_finishes_immediately);
  RUN_TEST(test_fsm_duplicate_blocks_store_and_reports_existing_id);
  RUN_TEST(test_fsm_can_skip_duplicate_and_continue_queue);
  RUN_TEST(test_fsm_stores_at_id_minus_one_and_advances_after_release);
  RUN_TEST(test_fsm_waits_for_first_finger_removal_before_second_sample);
  RUN_TEST(test_fsm_verify_toggle_off_skips_post_store_scan);
  RUN_TEST(test_fsm_verify_toggle_on_requires_remove_then_matching_scan);
  RUN_TEST(test_fsm_verify_failure_is_visible_and_can_continue_without_deleting);
  RUN_TEST(test_fsm_no_finger_is_wait_not_an_error);
  RUN_TEST(test_fsm_pauses_after_three_transient_errors_and_can_retry);
  RUN_TEST(test_fsm_model_mismatch_restarts_first_finger_pair);
  RUN_TEST(test_fsm_rechecks_occupancy_before_store);
  RUN_TEST(test_fsm_begin_rejects_incomplete_roster_history);
  RUN_TEST(test_fsm_rechecks_roster_history_before_store);
  RUN_TEST(test_fsm_rechecks_roster_before_store);
  RUN_TEST(test_fsm_invalid_occupancy_before_store_blocks_write);
  RUN_TEST(test_fsm_ambiguous_store_invalidates_and_refresh_detects_success);
  RUN_TEST(test_fsm_occupancy_refresh_cannot_resume_with_incomplete_roster_history);
  RUN_TEST(test_fsm_ambiguous_store_refresh_without_target_requeues_student);
  RUN_TEST(test_fsm_skip_is_blocked_while_sensor_command_is_pending);
  RUN_TEST(test_fsm_cancel_waits_for_confirmed_finger_removal);
  RUN_TEST(test_fsm_cancel_drains_inflight_command_before_release_fence);
  RUN_TEST(test_fsm_cancel_sensor_fault_keeps_ownership_until_retry_and_release);
  RUN_TEST(test_fsm_pending_sensor_results_do_not_advance_or_consume_retries);
  RUN_TEST(test_fsm_cancel_during_pending_store_applies_positive_ack);
  RUN_TEST(test_fsm_cancel_during_ambiguous_store_invalidates_occupancy);
  RUN_TEST(test_fsm_pending_store_does_not_invalidate_occupancy_or_repeat_write);
  RUN_TEST(test_fsm_occupancy_refresh_does_not_requeue_prior_skips);
  return UNITY_END();
}
