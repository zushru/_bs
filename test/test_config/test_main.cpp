#include <unity.h>
#include <string.h>
#include "config_store.h"
#include "event_codec.h"
using namespace att;

void setUp() {}
void tearDown() {}

static Config good() {
  Config c = defaultConfig();
  c.params.utcOffsetMin = 330; c.params.lateMin = 20; c.params.scanLeadMin = 10;
  c.tt.day[4][0].start = 8 * 60 + 15; c.tt.day[4][0].end = 12 * 60;
  c.termStart = 100; c.termEnd = 400;
  c.autoSync = 0; c.soundMode = 2; c.minScore = 123; c.pinSet = 1;
  memset(c.pinHash, 0xA5, 32); memset(c.pinSalt, 0x5A, 8);
  strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}
static void blank(uint8_t* b) { memset(b, 0xFF, CFG_SLOT_SIZE); }
static void setStoredScanLead(uint8_t* slot, uint16_t lead) {
  slot[15] = (uint8_t)lead;
  slot[16] = (uint8_t)(lead >> 8);
  const uint16_t crc = crc16(slot, 184);
  slot[184] = (uint8_t)crc;
  slot[185] = (uint8_t)(crc >> 8);
}

void test_default_scan_window_is_thirty_minutes() {
  const Config c = defaultConfig();
  TEST_ASSERT_EQUAL_UINT16(30, c.params.scanLeadMin);
}

void test_legacy_fifteen_minute_scan_lead_is_normalized() {
  Config encoded = good(), decoded = defaultConfig();
  uint8_t slot[CFG_SLOT_SIZE];
  uint32_t seq = 0;
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(encoded, 11, slot));
  setStoredScanLead(slot, 15);  // v1 slot written before the new 30-minute default
  TEST_ASSERT_EQUAL_INT(CD_OK, decodeConfig(slot, decoded, seq));
  TEST_ASSERT_EQUAL_UINT16(30, decoded.params.scanLeadMin);
  TEST_ASSERT_EQUAL_UINT32(11, seq);
}

void test_same_generation_slots_with_different_legacy_leads_still_fault() {
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  Config out = defaultConfig();
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(good(), 9, a));
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(good(), 9, b));
  setStoredScanLead(a, 15);
  setStoredScanLead(b, 30);
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pickSlot(a, b, out).boot);
}

void test_roundtrip_and_layout() {
  Config c = good(), d = defaultConfig();
  uint8_t s[CFG_SLOT_SIZE];
  uint32_t seq = 0;
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 0x01020304u, s));
  TEST_ASSERT_TRUE(memcmp(s, "ATTC", 4) == 0);
  TEST_ASSERT_EQUAL_UINT8(1, s[4]);
  TEST_ASSERT_EQUAL_UINT8(173, s[5]);
  TEST_ASSERT_EQUAL_UINT8(0, s[6]);
  TEST_ASSERT_EQUAL_UINT8(0x04, s[7]);
  TEST_ASSERT_EQUAL_UINT8(0x01, s[10]);
  TEST_ASSERT_EQUAL_UINT16(crc16(s, 184), (uint16_t)(s[184] | (s[185] << 8)));
  for (int i = 186; i < 192; i++) TEST_ASSERT_EQUAL_UINT8(0xFF, s[i]);
  TEST_ASSERT_EQUAL_INT(CD_OK, decodeConfig(s, d, seq));
  TEST_ASSERT_TRUE(configEqual(c, d));
  TEST_ASSERT_EQUAL_UINT32(0x01020304u, seq);
}
void test_both_blank_is_unprovisioned() {
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  blank(a); blank(b);
  Config out = defaultConfig();
  CfgPick p = pickSlot(a, b, out);
  TEST_ASSERT_EQUAL_INT(CB_UNPROVISIONED, p.boot);
  TEST_ASSERT_EQUAL_INT(-1, p.active);
}
void test_bit_flip_falls_back_to_other_slot() {
  Config c1 = good(), c2 = good(), out = defaultConfig();
  c2.minScore = 200;
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  encodeConfig(c1, 1, a); encodeConfig(c2, 2, b);
  b[40] ^= 0x10;
  uint32_t sq = 0;
  Config tmp = defaultConfig();
  TEST_ASSERT_EQUAL_INT(CD_BAD_CRC, decodeConfig(b, tmp, sq));
  CfgPick p = pickSlot(a, b, out);
  TEST_ASSERT_EQUAL_INT(CB_OK, p.boot);
  TEST_ASSERT_EQUAL_INT(0, p.active);
  TEST_ASSERT_EQUAL_UINT32(1, p.seq);
  TEST_ASSERT_TRUE(configEqual(c1, out));
}
void test_torn_write_keeps_old_slot() {
  Config c1 = good(), c2 = good(), out = defaultConfig();
  c2.soundMode = 1;
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  encodeConfig(c1, 7, a); encodeConfig(c2, 8, b);
  memset(b + 90, 0xFF, CFG_SLOT_SIZE - 90);            // power cut mid-write
  CfgPick p = pickSlot(a, b, out);
  TEST_ASSERT_EQUAL_INT(CB_OK, p.boot);
  TEST_ASSERT_EQUAL_INT(0, p.active);
  TEST_ASSERT_TRUE(configEqual(c1, out));
  blank(b);                                            // first-ever write: other slot blank
  p = pickSlot(a, b, out);
  TEST_ASSERT_EQUAL_INT(0, p.active);
}
void test_higher_seq_wins_any_order() {
  Config c1 = good(), c2 = good(), out = defaultConfig();
  c2.minScore = 999;
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  encodeConfig(c1, 5, a); encodeConfig(c2, 3, b);
  CfgPick p = pickSlot(a, b, out);
  TEST_ASSERT_EQUAL_INT(0, p.active);
  TEST_ASSERT_EQUAL_UINT16(123, out.minScore);
  p = pickSlot(b, a, out);
  TEST_ASSERT_EQUAL_INT(1, p.active);
  TEST_ASSERT_EQUAL_UINT32(5, p.seq);
  blank(b);                                            // one valid, one blank
  p = pickSlot(b, a, out);
  TEST_ASSERT_EQUAL_INT(CB_OK, p.boot);
  TEST_ASSERT_EQUAL_INT(1, p.active);
}
void test_equal_seq_different_payload_is_fault() {
  Config c1 = good(), c2 = good(), out = defaultConfig();
  c2.minScore = 7;
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE], a2[CFG_SLOT_SIZE];
  encodeConfig(c1, 4, a); encodeConfig(c2, 4, b); encodeConfig(c1, 4, a2);
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pickSlot(a, b, out).boot);
  TEST_ASSERT_EQUAL_INT(CB_OK, pickSlot(a, a2, out).boot);   // identical copies are fine
}
void test_both_invalid_is_fault_and_out_untouched() {
  uint8_t a[CFG_SLOT_SIZE], b[CFG_SLOT_SIZE];
  encodeConfig(good(), 1, a);
  a[60] ^= 0x01;
  memset(b, 0x00, CFG_SLOT_SIZE);
  Config out = defaultConfig();
  out.minScore = 4242;
  CfgPick p = pickSlot(a, b, out);
  TEST_ASSERT_EQUAL_INT(CB_FAULT, p.boot);
  TEST_ASSERT_EQUAL_INT(-1, p.active);
  TEST_ASSERT_EQUAL_UINT16(4242, out.minScore);
  blank(b);                                            // corrupt + blank is still a fault
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pickSlot(a, b, out).boot);
}
void test_bad_version_and_bad_magic() {
  uint8_t s[CFG_SLOT_SIZE];
  Config c = defaultConfig();
  uint32_t seq = 0;
  encodeConfig(good(), 1, s);
  s[4] = 2;
  TEST_ASSERT_EQUAL_INT(CD_BAD_VER, decodeConfig(s, c, seq));
  encodeConfig(good(), 1, s);
  s[0] = 'X';
  TEST_ASSERT_EQUAL_INT(CD_BAD_MAGIC, decodeConfig(s, c, seq));
}
void test_validate_rejects_each_rule() {
  Config c = good();
  TEST_ASSERT_TRUE(validateConfig(c));
  TEST_ASSERT_FALSE(validateConfig(defaultConfig()));            // factory default is never valid
  c = good(); c.params.utcOffsetMin = -721; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.params.utcOffsetMin = 841; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.params.scanLeadMin = 61; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.params.lateMin = 241; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.params.lateMin = 180; TEST_ASSERT_FALSE(validateConfig(c));   // not < shortest slot (180)
  c = good(); c.params.lateMin = 179; TEST_ASSERT_TRUE(validateConfig(c));
  c = good(); c.tt.day[0][0].end = c.tt.day[0][0].start; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.termStart = -1; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.termEnd = c.termStart; TEST_ASSERT_TRUE(validateConfig(c));  // one-day inclusive term
  c = good(); c.termEnd = c.termStart - 1; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.termEnd = MAX_TERM_DAY; TEST_ASSERT_TRUE(validateConfig(c));
  c = good(); c.termStart = MAX_TERM_DAY; c.termEnd = MAX_TERM_DAY; TEST_ASSERT_TRUE(validateConfig(c));
  c = good(); c.termEnd = MAX_TERM_DAY + 1; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.termStart = MAX_TERM_DAY + 1; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.soundMode = 3; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.autoSync = 2; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); c.pinSet = 0; TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); memset(c.pinHash, 0, 32); TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); memset(c.wifiPass, 0, 64); strcpy(c.wifiPass, "1234567"); TEST_ASSERT_FALSE(validateConfig(c));
  c = good(); memset(c.wifiPass, 'a', 64); TEST_ASSERT_FALSE(validateConfig(c));        // no NUL
  c = good(); memset(c.wifiPass, 0, 64); memset(c.wifiPass, 'a', 63); TEST_ASSERT_TRUE(validateConfig(c));
  c = good(); c.wifiPass[3] = 0x07; TEST_ASSERT_FALSE(validateConfig(c));                // control char
  c = good(); c.wifiPass[40] = 'x'; TEST_ASSERT_FALSE(validateConfig(c));                // junk after NUL
}
void test_encode_refuses_invalid() {
  uint8_t s[CFG_SLOT_SIZE];
  memset(s, 0x11, sizeof s);
  Config c = good();
  c.soundMode = 9;
  TEST_ASSERT_EQUAL_UINT32(0, encodeConfig(c, 1, s));
  TEST_ASSERT_EQUAL_UINT8(0x11, s[0]);
  TEST_ASSERT_EQUAL_UINT8(0x11, s[191]);
}
void test_can_apply_matrix() {
  Config cur = good(), n = good();
  TEST_ASSERT_EQUAL_INT(CH_OK, canApply(cur, n, true, true));              // identical: always ok
  n.soundMode = 0; n.autoSync = 1; n.minScore = 5; n.pinHash[0] ^= 1; n.wifiPass[0] = 'Z';
  TEST_ASSERT_EQUAL_INT(CH_OK, canApply(cur, n, true, true));              // non-schedule: always ok
  const int kinds = 6;
  for (int k = 0; k < kinds; k++) {
    n = good();
    if (k == 0) n.params.lateMin = 25;
    if (k == 1) n.params.scanLeadMin = 5;
    if (k == 2) n.params.utcOffsetMin = 0;
    if (k == 3) n.tt.day[1][1].end += 10;
    if (k == 4) n.termStart = 101;
    if (k == 5) n.termEnd = 401;
    TEST_ASSERT_EQUAL_INT(CH_REFUSED_WINDOW_OPEN, canApply(cur, n, true, false));
    TEST_ASSERT_EQUAL_INT(CH_REFUSED_WINDOW_OPEN, canApply(cur, n, true, true));
    TEST_ASSERT_EQUAL_INT(CH_REFUSED_UNCLOSED, canApply(cur, n, false, true));
    TEST_ASSERT_EQUAL_INT(CH_OK, canApply(cur, n, false, false));
  }
}
void test_config_equal_detects_each_field() {
  Config a = good(), b = good();
  TEST_ASSERT_TRUE(configEqual(a, b));
  b = good(); b.params.utcOffsetMin++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.params.lateMin++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.params.scanLeadMin++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.tt.day[2][0].start++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.tt.day[3][1].enabled = !b.tt.day[3][1].enabled; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.termStart++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.termEnd++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.autoSync ^= 1; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.soundMode = 1; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.minScore++; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.pinHash[31] ^= 1; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.pinSalt[7] ^= 1; TEST_ASSERT_FALSE(configEqual(a, b));
  b = good(); b.wifiPass[0] = 'Q'; TEST_ASSERT_FALSE(configEqual(a, b));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_scan_window_is_thirty_minutes);
  RUN_TEST(test_legacy_fifteen_minute_scan_lead_is_normalized);
  RUN_TEST(test_same_generation_slots_with_different_legacy_leads_still_fault);
  RUN_TEST(test_roundtrip_and_layout);
  RUN_TEST(test_both_blank_is_unprovisioned);
  RUN_TEST(test_bit_flip_falls_back_to_other_slot);
  RUN_TEST(test_torn_write_keeps_old_slot);
  RUN_TEST(test_higher_seq_wins_any_order);
  RUN_TEST(test_equal_seq_different_payload_is_fault);
  RUN_TEST(test_both_invalid_is_fault_and_out_untouched);
  RUN_TEST(test_bad_version_and_bad_magic);
  RUN_TEST(test_validate_rejects_each_rule);
  RUN_TEST(test_encode_refuses_invalid);
  RUN_TEST(test_can_apply_matrix);
  RUN_TEST(test_config_equal_detects_each_field);
  return UNITY_END();
}
