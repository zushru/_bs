#include "config_store.h"
#include "event_codec.h"
#include <string.h>
namespace att {
namespace {
const uint8_t MAGIC[4] = {'A', 'T', 'T', 'C'};
const size_t HDR = 11;                              // magic4 ver1 len2 seq4
const size_t USED = HDR + CFG_PAYLOAD_LEN + 2;      // 186

void p16(uint8_t* o, uint16_t v) { o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); }
void p32(uint8_t* o, uint32_t v) { p16(o, (uint16_t)v); p16(o + 2, (uint16_t)(v >> 16)); }
uint16_t g16(const uint8_t* i) { return (uint16_t)(i[0] | (i[1] << 8)); }
uint32_t g32(const uint8_t* i) { return (uint32_t)g16(i) | ((uint32_t)g16(i + 2) << 16); }

bool allFF(const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n; i++) if (d[i] != 0xFF) return false;
  return true;
}
bool allZero(const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n; i++) if (d[i] != 0) return false;
  return true;
}
bool slotEq(const Slot& a, const Slot& b) {
  return a.start == b.start && a.end == b.end && a.enabled == b.enabled;
}
bool scheduleSame(const Config& a, const Config& b) {
  if (a.params.utcOffsetMin != b.params.utcOffsetMin || a.params.lateMin != b.params.lateMin ||
      a.params.scanLeadMin != b.params.scanLeadMin) return false;
  if (a.termStart != b.termStart || a.termEnd != b.termEnd) return false;
  for (int d = 0; d < 5; d++)
    for (int s = 0; s < 2; s++)
      if (!slotEq(a.tt.day[d][s], b.tt.day[d][s])) return false;
  return true;
}
}  // namespace

Config defaultConfig() {
  Config c;
  memset(&c, 0, sizeof c);
  c.params.utcOffsetMin = 480;
  c.params.lateMin = 30;
  c.params.scanLeadMin = 30;
  c.tt = defaultTimetable();
  c.termStart = -1;
  c.termEnd = -1;
  c.autoSync = 1;
  return c;
}

bool validateConfig(const Config& c) {
  if (c.params.utcOffsetMin < -720 || c.params.utcOffsetMin > 840) return false;
  if (c.params.scanLeadMin > 60 || c.params.lateMin > 240) return false;
  if (!validateTimetable(c.tt, c.params.scanLeadMin)) return false;
  for (int d = 0; d < 5; d++)
    for (int s = 0; s < 2; s++) {
      const Slot& x = c.tt.day[d][s];
      if (x.enabled && c.params.lateMin >= (uint16_t)(x.end - x.start)) return false;
    }
  if (c.termStart < 0 || c.termEnd <= c.termStart) return false;
  if (c.autoSync > 1 || c.soundMode > 2) return false;
  if (c.pinSet != 1 || allZero(c.pinHash, sizeof c.pinHash)) return false;
  size_t n = 0;
  while (n < sizeof c.wifiPass && c.wifiPass[n] != 0) n++;
  if (n < 8 || n > 63) return false;
  for (size_t i = 0; i < n; i++)
    if ((uint8_t)c.wifiPass[i] < 0x20 || (uint8_t)c.wifiPass[i] > 0x7E) return false;
  for (size_t i = n; i < sizeof c.wifiPass; i++) if (c.wifiPass[i] != 0) return false;
  return true;
}

bool configEqual(const Config& a, const Config& b) {
  return scheduleSame(a, b) && a.autoSync == b.autoSync && a.soundMode == b.soundMode &&
         a.minScore == b.minScore && a.pinSet == b.pinSet &&
         memcmp(a.pinHash, b.pinHash, sizeof a.pinHash) == 0 &&
         memcmp(a.pinSalt, b.pinSalt, sizeof a.pinSalt) == 0 &&
         memcmp(a.wifiPass, b.wifiPass, sizeof a.wifiPass) == 0;
}

size_t encodeConfig(const Config& c, uint32_t seq, uint8_t out[CFG_SLOT_SIZE]) {
  if (!validateConfig(c)) return 0;
  memset(out, 0xFF, CFG_SLOT_SIZE);
  memcpy(out, MAGIC, 4);
  out[4] = CFG_SCHEMA_VER;
  p16(out + 5, (uint16_t)CFG_PAYLOAD_LEN);
  p32(out + 7, seq);
  uint8_t* p = out + HDR;
  p16(p + 0, (uint16_t)c.params.utcOffsetMin);
  p16(p + 2, c.params.lateMin);
  p16(p + 4, c.params.scanLeadMin);
  for (int d = 0; d < 5; d++)
    for (int s = 0; s < 2; s++) {
      uint8_t* q = p + 6 + (d * 2 + s) * 5;
      const Slot& x = c.tt.day[d][s];
      p16(q, x.start);
      p16(q + 2, x.end);
      q[4] = x.enabled ? 1 : 0;
    }
  p32(p + 56, (uint32_t)c.termStart);
  p32(p + 60, (uint32_t)c.termEnd);
  p[64] = c.autoSync;
  p[65] = c.soundMode;
  p16(p + 66, c.minScore);
  p[68] = c.pinSet;
  memcpy(p + 69, c.pinHash, 32);
  memcpy(p + 101, c.pinSalt, 8);
  memcpy(p + 109, c.wifiPass, 64);
  p16(out + HDR + CFG_PAYLOAD_LEN, crc16(out, HDR + CFG_PAYLOAD_LEN));
  return USED;
}

CfgDec decodeConfig(const uint8_t in[CFG_SLOT_SIZE], Config& c, uint32_t& seq) {
  if (allFF(in, CFG_SLOT_SIZE)) return CD_BLANK;
  if (memcmp(in, MAGIC, 4) != 0) return CD_BAD_MAGIC;
  if (in[4] != CFG_SCHEMA_VER) return CD_BAD_VER;
  if (g16(in + 5) != CFG_PAYLOAD_LEN) return CD_BAD_RANGE;
  if (crc16(in, HDR + CFG_PAYLOAD_LEN) != g16(in + HDR + CFG_PAYLOAD_LEN)) return CD_BAD_CRC;
  const uint8_t* p = in + HDR;
  Config t;
  memset(&t, 0, sizeof t);
  t.params.utcOffsetMin = (int16_t)g16(p + 0);
  t.params.lateMin = g16(p + 2);
  t.params.scanLeadMin = g16(p + 4);
  // The old product default was 15 minutes. The new default is 30; preserve
  // explicitly configured non-default values and keep the v1 slot layout.
  if (t.params.scanLeadMin == 15) t.params.scanLeadMin = 30;
  for (int d = 0; d < 5; d++)
    for (int s = 0; s < 2; s++) {
      const uint8_t* q = p + 6 + (d * 2 + s) * 5;
      if (q[4] > 1) return CD_BAD_RANGE;
      t.tt.day[d][s].start = g16(q);
      t.tt.day[d][s].end = g16(q + 2);
      t.tt.day[d][s].enabled = q[4] == 1;
    }
  t.termStart = (int32_t)g32(p + 56);
  t.termEnd = (int32_t)g32(p + 60);
  t.autoSync = p[64];
  t.soundMode = p[65];
  t.minScore = g16(p + 66);
  t.pinSet = p[68];
  memcpy(t.pinHash, p + 69, 32);
  memcpy(t.pinSalt, p + 101, 8);
  memcpy(t.wifiPass, p + 109, 64);
  if (!validateConfig(t)) return CD_BAD_RANGE;
  c = t;
  seq = g32(in + 7);
  return CD_OK;
}

CfgPick pickSlot(const uint8_t a[CFG_SLOT_SIZE], const uint8_t b[CFG_SLOT_SIZE], Config& out) {
  Config ca, cb;
  uint32_t sa = 0, sb = 0;
  CfgDec da = decodeConfig(a, ca, sa);
  CfgDec db = decodeConfig(b, cb, sb);
  CfgPick r = {CB_FAULT, -1, 0};
  if (da == CD_BLANK && db == CD_BLANK) { r.boot = CB_UNPROVISIONED; return r; }
  bool oa = da == CD_OK, ob = db == CD_OK;
  if (oa && ob) {
    if (sa == sb) {
      // Compare the encoded records as well as normalized configs. A migration
      // must not hide different same-generation source bytes (e.g. old lead 15
      // versus 30) by decoding both to the same effective setting.
      if (memcmp(a, b, USED) != 0 || !configEqual(ca, cb)) return r;
      out = ca; return CfgPick{CB_OK, 0, sa};
    }
    if (sa > sb) { out = ca; return CfgPick{CB_OK, 0, sa}; }
    out = cb; return CfgPick{CB_OK, 1, sb};
  }
  if (oa) { out = ca; return CfgPick{CB_OK, 0, sa}; }
  if (ob) { out = cb; return CfgPick{CB_OK, 1, sb}; }
  return r;                                         // non-blank slots, none valid
}

CfgChange canApply(const Config& cur, const Config& next, bool windowOpen, bool unclosedPast) {
  if (scheduleSame(cur, next)) return CH_OK;
  if (windowOpen) return CH_REFUSED_WINDOW_OPEN;
  if (unclosedPast) return CH_REFUSED_UNCLOSED;
  return CH_OK;
}
}  // namespace att
