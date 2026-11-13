#include "backup_format.h"
#include "event_codec.h"
#include <string.h>
namespace att {
void Crc32::add(const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n; i++) {
    s ^= d[i];
    for (int k = 0; k < 8; k++) s = (s >> 1) ^ (0xEDB88320u & (0u - (s & 1u)));
  }
}
uint32_t crc32(const uint8_t* d, size_t n) { Crc32 c; c.add(d, n); return c.value(); }
namespace {
void p16(uint8_t* o, uint16_t v) { o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); }
void p32(uint8_t* o, uint32_t v) { p16(o, (uint16_t)v); p16(o + 2, (uint16_t)(v >> 16)); }
uint16_t g16(const uint8_t* i) { return (uint16_t)(i[0] | (i[1] << 8)); }
uint32_t g32(const uint8_t* i) { return (uint32_t)g16(i) | ((uint32_t)g16(i + 2) << 16); }
const uint8_t MAGIC_O[4] = {'A', 'T', 'T', 'B'};
const uint8_t MAGIC_I[4] = {'A', 'T', 'T', 'I'};
void chunkCtx(const uint8_t* hdr, const uint8_t base[12], uint32_t idx, uint8_t fin, uint8_t nonce[12], uint8_t aad[BK_HDR + 5]) {
  memcpy(nonce, base, 12);
  for (int i = 0; i < 4; i++) nonce[8 + i] ^= (uint8_t)(idx >> (8 * i));
  memcpy(aad, hdr, BK_HDR);
  p32(aad + BK_HDR, idx);
  aad[BK_HDR + 4] = fin;
}
}  // namespace

// ---------------- BackupWriter ----------------
bool BackupWriter::emit_(const uint8_t* d, size_t n) {
  if (!out_.write(d, n)) { err_ = true; return false; }
  crc_.add(d, n); len_ += (uint32_t)n;
  return true;
}
bool BackupWriter::begin(const uint8_t* pass, size_t n, const uint8_t salt[16], const uint8_t nonce[12], uint32_t iter) {
  if (started_ || iter < BK_ITER_MIN || iter > BK_ITER_MAX) return false;
  memcpy(hdr_, MAGIC_O, 4); hdr_[4] = 1; p32(hdr_ + 5, iter);
  memcpy(hdr_ + 9, salt, 16); memcpy(hdr_ + 25, nonce, 12);
  if (!c_.deriveKey(pass, n, salt, iter, key_)) { err_ = true; return false; }
  started_ = true;
  return emit_(hdr_, BK_HDR);
}
bool BackupWriter::flush_(bool fin) {
  uint8_t nonce[12], aad[BK_HDR + 5], tag[BK_TAG], fh[3];
  chunkCtx(hdr_, hdr_ + 25, idx_, fin ? 1 : 0, nonce, aad);
  if (!c_.seal(key_, nonce, aad, sizeof aad, buf_, fill_, ct_, tag)) { err_ = true; return false; }
  fh[0] = fin ? 1 : 0; p16(fh + 1, (uint16_t)fill_);
  if (!emit_(fh, 3) || !emit_(ct_, fill_) || !emit_(tag, BK_TAG)) return false;
  idx_++; fill_ = 0;
  return true;
}
bool BackupWriter::write(const uint8_t* d, size_t n) {
  if (!started_ || done_ || err_) return false;
  while (n > 0) {
    if (fill_ == BK_CHUNK && !flush_(false)) return false;   // flush only when more data arrives
    size_t take = BK_CHUNK - fill_;
    if (take > n) take = n;
    memcpy(buf_ + fill_, d, take);
    fill_ += take; d += take; n -= take;
  }
  return true;
}
bool BackupWriter::finish() {
  if (!started_ || done_ || err_) return false;
  if (!flush_(true)) return false;
  done_ = true;
  return true;
}

// ---------------- BackupReader ----------------
bool BackupReader::step_() {
  if (st_ == 0) {                                   // header
    memcpy(hdr_, buf_, BK_HDR);
    uint32_t iter = g32(hdr_ + 5);
    if (memcmp(hdr_, MAGIC_O, 4) != 0 || hdr_[4] != 1 || iter < BK_ITER_MIN || iter > BK_ITER_MAX) return false;
    if (!c_.deriveKey(pass_, passLen_, hdr_ + 9, iter, key_)) return false;
    st_ = 1; need_ = 3;
  } else if (st_ == 1) {                            // chunk header
    if (buf_[0] > 1) return false;
    fin_ = buf_[0]; len_ = g16(buf_ + 1);
    if (len_ > BK_CHUNK) return false;
    st_ = 2; need_ = len_ + BK_TAG;
  } else {                                          // chunk body
    uint8_t nonce[12], aad[BK_HDR + 5];
    chunkCtx(hdr_, hdr_ + 25, idx_, fin_, nonce, aad);
    if (!c_.open(key_, nonce, aad, sizeof aad, buf_, len_, buf_ + len_, plain_)) return false;
    if (len_ > 0 && !out_.write(plain_, len_)) return false;
    idx_++;
    if (fin_) { done_ = true; st_ = 3; need_ = 1; }
    else { st_ = 1; need_ = 3; }
  }
  have_ = 0;
  return true;
}
bool BackupReader::feed(const uint8_t* d, size_t n) {
  if (err_) return false;
  while (n > 0) {
    if (st_ == 3) { err_ = true; return false; }    // bytes after the final chunk
    size_t take = need_ - have_;
    if (take > n) take = n;
    memcpy(buf_ + have_, d, take);
    have_ += take; d += take; n -= take;
    if (have_ == need_ && !step_()) { err_ = true; return false; }
  }
  return true;
}

// ---------------- InnerBuilder ----------------
bool InnerBuilder::put_(const uint8_t* d, size_t n) {
  if (err_ || !out_.write(d, n)) { err_ = true; return false; }
  crc_.add(d, n);
  return true;
}
bool InnerBuilder::begin(const Config& c, uint32_t cfgSeq, uint16_t fsVersion, uint32_t recordCount) {
  uint8_t h[10], slot[CFG_SLOT_SIZE], n4[4];
  memcpy(h, MAGIC_I, 4); h[4] = BK_INNER_VER; p16(h + 5, fsVersion); h[7] = 0; p16(h + 8, (uint16_t)CFG_SLOT_SIZE);
  if (encodeConfig(c, cfgSeq, slot) == 0) { err_ = true; return false; }
  p32(n4, recordCount);
  declared_ = recordCount; written_ = 0;
  return put_(h, 10) && put_(slot, CFG_SLOT_SIZE) && put_(n4, 4);
}
bool InnerBuilder::records(const uint8_t* d, size_t n) {
  if ((n % REC_SIZE) != 0 || written_ + n / REC_SIZE > declared_) { err_ = true; return false; }
  written_ += (uint32_t)(n / REC_SIZE);
  return put_(d, n);
}
bool InnerBuilder::finish() {
  uint8_t t[4]; p32(t, crc_.value());
  if (err_ || written_ != declared_) { err_ = true; return false; }
  return out_.write(t, 4);
}

// ---------------- RestoreValidator ----------------
enum { V_HDR, V_CFG, V_NREC, V_REC, V_CRC, V_DONE };
RestoreValidator::RestoreValidator(IStorage* staging, uint16_t expectFsVersion)
    : stg_(staging), fsv_(expectFsVersion), nobs_(0), st_(V_HDR), need_(10), have_(0),
      cfgSeq_(0), nrec_(0), left_(0), lastSeq_(0), prevMore_(false), ok_(false), err_(false) {
  cfg_ = defaultConfig();
}
bool RestoreValidator::addObserver(LogObserver* o) {
  if (!o || nobs_ >= 4 || st_ != V_HDR || have_ != 0) return false;   // before any byte is fed
  obs_[nobs_++] = o;
  o->onReset();
  return true;
}
bool RestoreValidator::step_() {
  if (st_ != V_CRC) crc_.add(buf_, need_);
  switch (st_) {
    case V_HDR:
      if (memcmp(buf_, MAGIC_I, 4) != 0 || buf_[4] != BK_INNER_VER || g16(buf_ + 5) != fsv_ ||
          buf_[7] != 0 || g16(buf_ + 8) != CFG_SLOT_SIZE) return fail_();
      st_ = V_CFG; need_ = CFG_SLOT_SIZE; break;
    case V_CFG: {
      Config c; uint32_t seq = 0;
      if (decodeConfig(buf_, c, seq) != CD_OK) return fail_();
      cfg_ = c; cfgSeq_ = seq;
      st_ = V_NREC; need_ = 4; break;
    }
    case V_NREC:
      nrec_ = g32(buf_);
      if (nrec_ > 1000000u) return fail_();
      left_ = nrec_;
      if (left_ == 0) { st_ = V_CRC; need_ = 4; } else { st_ = V_REC; need_ = REC_SIZE; }
      break;
    case V_REC: {
      Event e;
      if (!decode(buf_, e) || e.seq != lastSeq_ + 1) return fail_();
      if (stg_ && !stg_->append(buf_, REC_SIZE)) return fail_();
      recordsCrc_.add(buf_, REC_SIZE);
      lastSeq_ = e.seq; prevMore_ = flagMore(e.flags);
      for (size_t i = 0; i < nobs_; i++) obs_[i]->onRecord(e);
      if (--left_ == 0) {
        if (prevMore_) return fail_();                // unfinished commit group
        st_ = V_CRC; need_ = 4;
      }
      break;
    }
    default:                                          // V_CRC
      if (g32(buf_) != crc_.value()) return fail_();
      ok_ = true; st_ = V_DONE; need_ = 1; break;
  }
  have_ = 0;
  return true;
}
bool RestoreValidator::write(const uint8_t* d, size_t n) {
  if (err_) return false;
  while (n > 0) {
    if (st_ == V_DONE) return fail_();                // trailing bytes
    size_t take = need_ - have_;
    if (take > n) take = n;
    memcpy(buf_ + have_, d, take);
    have_ += take; d += take; n -= take;
    if (have_ == need_ && !step_()) return false;
  }
  return true;
}
}
