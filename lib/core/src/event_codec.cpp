#include "event_codec.h"
namespace att {
uint16_t crc16(const uint8_t* d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)d[i] << 8;
    for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}
static void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static uint16_t get16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t* p) { return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16); }

void encode(const Event& e, uint8_t out[REC_SIZE]) {
  out[0] = e.type;
  out[1] = e.flags;
  put16(out + 2, e.a);
  put32(out + 4, e.epoch);
  put32(out + 8, e.seq);
  put16(out + 12, e.b);
  put16(out + 14, crc16(out, 14));
}
bool decode(const uint8_t in[REC_SIZE], Event& e) {
  if (crc16(in, 14) != get16(in + 14)) return false;
  if (in[0] == EV_NONE || in[0] > EV_MAX) return false;
  e.type = in[0];
  e.flags = in[1];
  e.a = get16(in + 2);
  e.epoch = get32(in + 4);
  e.seq = get32(in + 8);
  e.b = get16(in + 12);
  return true;
}
uint8_t makeFlags(uint8_t session, uint8_t status, bool coverage, bool more, bool wholeDay) {
  return (uint8_t)((session & 1) | ((status & 3) << 1) | (coverage ? FL_COVERAGE : 0) |
                   (more ? FL_MORE : 0) | (wholeDay ? FL_WHOLEDAY : 0));
}
uint8_t flagSession(uint8_t f) { return f & 1; }
uint8_t flagStatus(uint8_t f) { return (f >> 1) & 3; }
bool flagCoverage(uint8_t f) { return (f & FL_COVERAGE) != 0; }
bool flagMore(uint8_t f) { return (f & FL_MORE) != 0; }
bool flagWholeDay(uint8_t f) { return (f & FL_WHOLEDAY) != 0; }

Event makePayload(uint64_t bits, uint32_t seq, bool more) {
  Event e;
  e.type = EV_PAYLOAD;
  e.flags = more ? FL_MORE : 0;
  e.a = (uint16_t)(bits & 0xFFFF);
  e.epoch = (uint32_t)((bits >> 16) & 0xFFFFFFFFu);
  e.seq = seq;
  e.b = (uint16_t)(bits >> 48);
  return e;
}
uint64_t payloadBits(const Event& e) {
  return (uint64_t)e.a | ((uint64_t)e.epoch << 16) | ((uint64_t)e.b << 48);
}
uint16_t payloadCount(uint16_t cap) { return (uint16_t)((cap + 63) / 64); }
void bitmapSet(uint64_t* w, uint16_t id) { uint16_t i = (uint16_t)(id - 1); w[i / 64] |= (uint64_t)1 << (i % 64); }
bool bitmapTest(const uint64_t* w, uint16_t id) { uint16_t i = (uint16_t)(id - 1); return (w[i / 64] >> (i % 64)) & 1; }
}
