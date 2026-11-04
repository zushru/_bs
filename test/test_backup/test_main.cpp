#include <unity.h>
#include <string.h>
#include <vector>
#include "backup_format.h"
#include "log_store.h"
using namespace att;

void setUp() {}
void tearDown() {}

// Toy cipher: exercises the container logic, NOT real crypto. Adapter uses mbedtls.
struct FakeCipher : ICipher {
  static uint64_t mix(uint64_t h, uint8_t b) { h ^= b; h *= 1099511628211ull; return h; }
  bool deriveKey(const uint8_t* p, size_t n, const uint8_t s[16], uint32_t it, uint8_t k[32]) override {
    uint64_t h = 1469598103934665603ull ^ it;
    for (size_t i = 0; i < n; i++) h = mix(h, p[i]);
    for (int i = 0; i < 16; i++) h = mix(h, s[i]);
    for (int i = 0; i < 32; i++) { h = mix(h, (uint8_t)i); k[i] = (uint8_t)(h >> 24); }
    return true;
  }
  void tagOf(const uint8_t* k, const uint8_t* nc, const uint8_t* aad, size_t al, const uint8_t* ct, size_t n, uint8_t t[16]) {
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 32; i++) h = mix(h, k[i]);
    for (int i = 0; i < 12; i++) h = mix(h, nc[i]);
    for (size_t i = 0; i < al; i++) h = mix(h, aad[i]);
    for (size_t i = 0; i < n; i++) h = mix(h, ct[i]);
    for (int i = 0; i < 16; i++) { h = mix(h, (uint8_t)(i + 77)); t[i] = (uint8_t)(h >> 16); }
  }
  void xorStream(const uint8_t* k, const uint8_t* nc, const uint8_t* in, size_t n, uint8_t* out) {
    uint64_t h = 88172645463325252ull;
    for (int i = 0; i < 32; i++) h = mix(h, k[i]);
    for (int i = 0; i < 12; i++) h = mix(h, nc[i]);
    for (size_t i = 0; i < n; i++) { h = h * 6364136223846793005ull + 1442695040888963407ull; out[i] = in[i] ^ (uint8_t)(h >> 33); }
  }
  bool seal(const uint8_t* k, const uint8_t* nc, const uint8_t* aad, size_t al, const uint8_t* in, size_t n, uint8_t* out, uint8_t* tag) override {
    xorStream(k, nc, in, n, out); tagOf(k, nc, aad, al, out, n, tag); return true;
  }
  bool open(const uint8_t* k, const uint8_t* nc, const uint8_t* aad, size_t al, const uint8_t* in, size_t n, const uint8_t* tag, uint8_t* out) override {
    uint8_t t[16]; tagOf(k, nc, aad, al, in, n, t);
    if (memcmp(t, tag, 16) != 0) return false;
    xorStream(k, nc, in, n, out); return true;
  }
};
struct Buf : IByteSink {
  std::vector<uint8_t> v;
  bool write(const uint8_t* d, size_t n) override { v.insert(v.end(), d, d + n); return true; }
};
struct MemStore : IStorage {
  std::vector<uint8_t> d;
  size_t size() override { return d.size(); }
  bool read(size_t o, uint8_t* b, size_t n) override { if (o + n > d.size()) return false; memcpy(b, d.data() + o, n); return true; }
  bool append(const uint8_t* b, size_t n) override { d.insert(d.end(), b, b + n); return true; }
  bool truncate(size_t n) override { d.resize(n); return true; }
};

static const uint8_t SALT[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static const uint8_t NONCE[12] = {9,8,7,6,5,4,3,2,1,0,1,2};
static const uint8_t PASS[] = {'p','a','s','s','w','o','r','d','1'};
static const uint16_t FSV = 3;

static std::vector<uint8_t> pattern(size_t n) { std::vector<uint8_t> v(n); for (size_t i = 0; i < n; i++) v[i] = (uint8_t)(i * 31 + 7); return v; }
static std::vector<uint8_t> seal(FakeCipher& c, const std::vector<uint8_t>& plain, BackupWriter** keep = nullptr) {
  (void)keep;
  Buf out;
  BackupWriter w(c, out);
  w.begin(PASS, sizeof PASS, SALT, NONCE, 1000);
  w.write(plain.data(), plain.size());
  w.finish();
  return out.v;
}
static Config goodCfg() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 0xA5, 32); strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}
// A real log of 12 records through LogStore -> raw bytes
static std::vector<uint8_t> makeLog(uint32_t* count) {
  MemStore ms; LogStore ls(ms); ls.open();
  for (int i = 0; i < 6; i++) { Event e[2] = {Event{EV_SCAN, 0, (uint16_t)(i + 1), (uint32_t)(1000 + i), 0, 0}, Event{EV_MARK, 0, (uint16_t)(i + 1), (uint32_t)(2000 + i), 0, 5}}; ls.append(e, 2); }
  *count = (uint32_t)(ms.d.size() / REC_SIZE);
  return ms.d;
}
static std::vector<uint8_t> makeInner(uint16_t fsv, const std::vector<uint8_t>& log, uint32_t nrec) {
  Buf b; InnerBuilder ib(b);
  ib.begin(goodCfg(), 4, fsv, nrec);
  ib.records(log.data(), log.size());
  ib.finish();
  return b.v;
}
static bool validate(const std::vector<uint8_t>& in, RestoreValidator& v, size_t step) {
  for (size_t o = 0; o < in.size(); o += step) {
    size_t n = in.size() - o < step ? in.size() - o : step;
    if (!v.write(in.data() + o, n)) return false;
  }
  return true;
}

void test_crc32_vector() {
  const uint8_t s[] = {'1','2','3','4','5','6','7','8','9'};
  TEST_ASSERT_TRUE(crc32(s, 9) == 0xCBF43926u);
  Crc32 c; c.add(s, 4); c.add(s + 4, 5);
  TEST_ASSERT_TRUE(c.value() == 0xCBF43926u);
}
void test_outer_roundtrip_any_split() {
  FakeCipher c;
  const size_t sizes[] = {0, 1, 4095, 4096, 4097, 10000};
  for (size_t si = 0; si < 6; si++) {
    std::vector<uint8_t> plain = pattern(sizes[si]);
    std::vector<uint8_t> enc = seal(c, plain);
    const size_t steps[] = {1, 7, 4113, 100000};
    for (size_t k = 0; k < 4; k++) {
      Buf got; BackupReader r(c, got, PASS, sizeof PASS);
      for (size_t o = 0; o < enc.size(); o += steps[k]) {
        size_t n = enc.size() - o < steps[k] ? enc.size() - o : steps[k];
        TEST_ASSERT_TRUE(r.feed(enc.data() + o, n));
      }
      TEST_ASSERT_TRUE(r.done());
      TEST_ASSERT_TRUE(got.v == plain);
    }
  }
}
void test_ack_crc_and_length() {
  FakeCipher c; Buf out; BackupWriter w(c, out);
  w.begin(PASS, sizeof PASS, SALT, NONCE, 1000);
  std::vector<uint8_t> p = pattern(9000);
  w.write(p.data(), p.size()); w.finish();
  TEST_ASSERT_EQUAL_UINT32(out.v.size(), w.length());
  TEST_ASSERT_TRUE(w.crc() == crc32(out.v.data(), out.v.size()));
  TEST_ASSERT_TRUE(ackMatches(w, w.crc(), w.length()));
  TEST_ASSERT_FALSE(ackMatches(w, w.crc() ^ 1, w.length()));
  TEST_ASSERT_FALSE(ackMatches(w, w.crc(), w.length() - 1));
  TEST_ASSERT_FALSE(w.write(p.data(), 1));                // closed
  TEST_ASSERT_TRUE(rolloverAllowed(50, 50, true, true));
  TEST_ASSERT_FALSE(rolloverAllowed(49, 50, true, true));
  TEST_ASSERT_FALSE(rolloverAllowed(50, 50, false, true));
  TEST_ASSERT_FALSE(rolloverAllowed(50, 50, true, false));
  TEST_ASSERT_FALSE(rolloverAllowed(0, 0, true, true));
}
void test_wrong_passphrase_emits_nothing() {
  FakeCipher c;
  std::vector<uint8_t> enc = seal(c, pattern(5000));
  const uint8_t bad[] = {'p','a','s','s','w','o','r','d','2'};
  Buf got; BackupReader r(c, got, bad, sizeof bad);
  TEST_ASSERT_FALSE(r.feed(enc.data(), enc.size()));
  TEST_ASSERT_EQUAL_UINT32(0, got.v.size());
  TEST_ASSERT_FALSE(r.done());
}
void test_tamper_header_and_ciphertext() {
  FakeCipher c;
  std::vector<uint8_t> enc = seal(c, pattern(5000));
  for (size_t pos = 0; pos < enc.size(); pos += 53) {
    std::vector<uint8_t> t = enc; t[pos] ^= 0x01;
    Buf got; BackupReader r(c, got, PASS, sizeof PASS);
    bool ok = r.feed(t.data(), t.size()) && r.done();
    TEST_ASSERT_FALSE(ok);
  }
}
void test_truncate_reorder_trailing() {
  FakeCipher c;
  std::vector<uint8_t> enc = seal(c, pattern(9000));       // 3 chunks: 4096 + 4096 + 808
  size_t c0 = BK_HDR, c1 = c0 + 3 + 4096 + 16;
  std::vector<uint8_t> cut(enc.begin(), enc.begin() + c1 + 3 + 4096 + 16);   // final chunk dropped
  { Buf g; BackupReader r(c, g, PASS, sizeof PASS); TEST_ASSERT_TRUE(r.feed(cut.data(), cut.size())); TEST_ASSERT_FALSE(r.done()); }
  std::vector<uint8_t> sw(enc.begin(), enc.begin() + BK_HDR);   // swap chunk 0 and 1
  sw.insert(sw.end(), enc.begin() + c1, enc.begin() + c1 + 3 + 4096 + 16);
  sw.insert(sw.end(), enc.begin() + c0, enc.begin() + c1);
  sw.insert(sw.end(), enc.begin() + c1 + 3 + 4096 + 16, enc.end());
  { Buf g; BackupReader r(c, g, PASS, sizeof PASS); TEST_ASSERT_FALSE(r.feed(sw.data(), sw.size())); }
  std::vector<uint8_t> tr = enc; tr.push_back(0);
  { Buf g; BackupReader r(c, g, PASS, sizeof PASS); TEST_ASSERT_FALSE(r.feed(tr.data(), tr.size())); }
}
void test_inner_validates_full_chain() {
  FakeCipher c; uint32_t n;
  std::vector<uint8_t> log = makeLog(&n);
  std::vector<uint8_t> enc = seal(c, makeInner(FSV, log, n));
  Buf plain; BackupReader r(c, plain, PASS, sizeof PASS);
  TEST_ASSERT_TRUE(r.feed(enc.data(), enc.size()));
  TEST_ASSERT_TRUE(r.done());
  MemStore stg; RestoreValidator v(&stg, FSV);
  TEST_ASSERT_TRUE(validate(plain.v, v, 5));
  TEST_ASSERT_TRUE(v.ok());
  TEST_ASSERT_EQUAL_UINT32(n, v.records());
  TEST_ASSERT_EQUAL_UINT32(n, v.lastSeq());
  TEST_ASSERT_EQUAL_UINT32(4, v.cfgSeq());
  TEST_ASSERT_TRUE(configEqual(v.config(), goodCfg()));
  TEST_ASSERT_TRUE(stg.d == log);
  MemStore st2; LogStore ls(st2);                         // staged copy opens as a healthy log
  st2.d = stg.d;
  OpenInfo oi = ls.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(n, oi.records);
}
void test_inner_rejections() {
  uint32_t n; std::vector<uint8_t> log = makeLog(&n);
  std::vector<uint8_t> good = makeInner(FSV, log, n);
  { MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_TRUE(validate(good, v, 1000)); TEST_ASSERT_TRUE(v.ok()); }
  { MemStore s; RestoreValidator v(&s, FSV + 1); TEST_ASSERT_FALSE(validate(good, v, 1000) && v.ok()); }      // fs version
  { std::vector<uint8_t> t = good; t[0] = 'X'; MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(t, v, 1000) && v.ok()); }
  { std::vector<uint8_t> t = good; t[10 + 20] ^= 1; MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(t, v, 1000) && v.ok()); }  // config
  { std::vector<uint8_t> t = good; t[good.size() - 1] ^= 1; MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(t, v, 1000) && v.ok()); }   // crc trailer
  { std::vector<uint8_t> t = good; t.push_back(0); MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(t, v, 1000) && v.ok()); }          // trailing
  { std::vector<uint8_t> t = good; t[10 + 192 + 4 + 16 * 3 + 5] ^= 1; MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(t, v, 1000) && v.ok()); } // record crc
  { std::vector<uint8_t> t = makeInner(FSV, log, n + 1); MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(t, v, 1000) && v.ok()); }       // count lies
  { std::vector<uint8_t> l2(log.begin(), log.begin() + 16 * 3); l2.insert(l2.end(), log.begin() + 16 * 5, log.end());   // seq gap
    MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(makeInner(FSV, l2, n - 2), v, 1000) && v.ok()); }
  { std::vector<uint8_t> l3(log.begin(), log.begin() + 16 * 3);                                                          // ends mid commit group
    MemStore s; RestoreValidator v(&s, FSV); TEST_ASSERT_FALSE(validate(makeInner(FSV, l3, 3), v, 1000) && v.ok()); }
}
void test_inner_empty_log_and_builder_rules() {
  std::vector<uint8_t> none;
  MemStore s2; RestoreValidator v2(&s2, FSV);
  TEST_ASSERT_TRUE(validate(makeInner(FSV, none, 0), v2, 64));
  TEST_ASSERT_TRUE(v2.ok());
  TEST_ASSERT_EQUAL_UINT32(0, v2.records());
  Buf b; InnerBuilder ib(b);
  Config bad = goodCfg(); bad.soundMode = 9;
  TEST_ASSERT_FALSE(ib.begin(bad, 1, FSV, 0));             // invalid config is never backed up
  uint32_t n; std::vector<uint8_t> log = makeLog(&n);
  std::vector<uint8_t> v1 = makeInner(FSV, log, n);        // a v1 stream (with a template count) must not validate
  std::vector<uint8_t> old = v1; old[4] = 1;
  MemStore s3; RestoreValidator v3(&s3, FSV);
  TEST_ASSERT_FALSE(validate(old, v3, 1000) && v3.ok());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_vector);
  RUN_TEST(test_outer_roundtrip_any_split);
  RUN_TEST(test_ack_crc_and_length);
  RUN_TEST(test_wrong_passphrase_emits_nothing);
  RUN_TEST(test_tamper_header_and_ciphertext);
  RUN_TEST(test_truncate_reorder_trailing);
  RUN_TEST(test_inner_validates_full_chain);
  RUN_TEST(test_inner_rejections);
  RUN_TEST(test_inner_empty_log_and_builder_rules);
  return UNITY_END();
}
