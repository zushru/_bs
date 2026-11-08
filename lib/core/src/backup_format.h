#pragma once
#include <stddef.h>
#include <stdint.h>
#include "config_store.h"
#include "istorage.h"
#include "log_store.h"
namespace att {
const size_t BK_CHUNK = 4096;       // plaintext bytes per encrypted chunk
const size_t BK_HDR = 37;           // "ATTB" ver1 iter4 salt16 nonce12
const size_t BK_TAG = 16;
const uint32_t BK_ITER_MIN = 1000;      // PBKDF2 iterations: writer and reader enforce the same range
const uint32_t BK_ITER_MAX = 5000000;

struct Crc32 {
  uint32_t s;
  Crc32() : s(0xFFFFFFFFu) {}
  void add(const uint8_t* d, size_t n);
  uint32_t value() const { return ~s; }
};
uint32_t crc32(const uint8_t* d, size_t n);

class IByteSink { public: virtual ~IByteSink() {} virtual bool write(const uint8_t* d, size_t n) = 0; };
// Adapter wraps mbedtls (PBKDF2-SHA256, AES-256-GCM) [UNVERIFIED]. Core only needs this shape.
class ICipher {
 public:
  virtual ~ICipher() {}
  virtual bool deriveKey(const uint8_t* pass, size_t n, const uint8_t salt[16], uint32_t iter, uint8_t key[32]) = 0;
  virtual bool seal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t* aad, size_t aadLen,
                    const uint8_t* in, size_t n, uint8_t* out, uint8_t tag[16]) = 0;
  virtual bool open(const uint8_t key[32], const uint8_t nonce[12], const uint8_t* aad, size_t aadLen,
                    const uint8_t* in, size_t n, const uint8_t tag[16], uint8_t* out) = 0;   // false = auth failure
};

// ---- Outer stream: header, then chunks [fin u8][len u16][ciphertext][tag16]. Each chunk is AEAD-sealed with
// AAD = header || chunk index || fin, nonce = base XOR index, so tamper, reorder, truncate and header edits all fail.
// ~8 KB object: allocate statically or on the heap.
class BackupWriter {
 public:
  BackupWriter(ICipher& c, IByteSink& out) : c_(c), out_(out), fill_(0), idx_(0), started_(false), done_(false), err_(false), len_(0) {}
  bool begin(const uint8_t* pass, size_t n, const uint8_t salt[16], const uint8_t nonce[12], uint32_t iter);
  bool write(const uint8_t* d, size_t n);      // plaintext
  bool finish();                               // final chunk; always emits at least one chunk
  uint32_t crc() const { return crc_.value(); }   // of every outer byte written: for BACKUP_ACK
  uint32_t length() const { return len_; }
  bool finished() const { return done_ && !err_; }   // final chunk written, no sink error
 private:
  bool flush_(bool fin);
  bool emit_(const uint8_t* d, size_t n);
  ICipher& c_; IByteSink& out_;
  uint8_t key_[32], hdr_[BK_HDR], buf_[BK_CHUNK], ct_[BK_CHUNK];
  size_t fill_; uint32_t idx_;
  bool started_, done_, err_;
  Crc32 crc_; uint32_t len_;
};
class BackupReader {
 public:
  // `pass` must stay valid until the reader is destroyed.
  BackupReader(ICipher& c, IByteSink& plain, const uint8_t* pass, size_t n)
      : c_(c), out_(plain), pass_(pass), passLen_(n), st_(0), need_(BK_HDR), have_(0), idx_(0), fin_(0), len_(0), err_(false), done_(false) {}
  bool feed(const uint8_t* d, size_t n);       // false = hard error; nothing unauthenticated is ever emitted
  bool done() const { return done_ && !err_; } // final chunk authenticated, no trailing bytes
 private:
  bool step_();
  ICipher& c_; IByteSink& out_;
  const uint8_t* pass_; size_t passLen_;
  uint8_t key_[32], hdr_[BK_HDR], buf_[BK_CHUNK + BK_TAG], plain_[BK_CHUNK];
  int st_; size_t need_, have_; uint32_t idx_; uint8_t fin_; size_t len_;
  bool err_, done_;
};

// ---- Inner plaintext: "ATTI" ver1 fsVer2 flags1 cfgLen2 | config slot (192) | nRec4 | records | crc32 of everything before it.
// The roster lives inside the records (EV_ROSTER groups). Fingerprint templates are deliberately NOT included.
const uint8_t BK_INNER_VER = 2;   // v2: no template section (templates are NOT backed up; re-enroll instead)
class InnerBuilder {
 public:
  explicit InnerBuilder(IByteSink& out) : out_(out), err_(false), declared_(0), written_(0) {}
  bool begin(const Config& c, uint32_t cfgSeq, uint16_t fsVersion, uint32_t recordCount);
  bool records(const uint8_t* d, size_t n);            // raw 16-byte records, in order
  bool finish();                                       // writes the crc32 trailer
 private:
  bool put_(const uint8_t* d, size_t n);
  IByteSink& out_; Crc32 crc_; bool err_;
  uint32_t declared_, written_;
};
// Streams a backup through the checks in restore order. Appends records to `staging`, never to the live log.
class RestoreValidator : public IByteSink {
 public:
  RestoreValidator(IStorage* staging, uint16_t expectFsVersion);
  bool write(const uint8_t* d, size_t n) override;     // false = invalid; stop and discard staging
  // Observers (e.g. a fresh RosterStore, LogIndex) see every validated record, so the adapter can reject a
  // backup whose log is well-formed but semantically broken (roster malformed(), index overflow) before swapping.
  bool addObserver(LogObserver* o);                    // up to 4; resets the observer first
  bool ok() const { return ok_ && !err_; }
  uint32_t records() const { return nrec_; }
  uint32_t lastSeq() const { return lastSeq_; }
  uint32_t recordsCrc() const { return recordsCrc_.value(); }
  uint32_t cfgSeq() const { return cfgSeq_; }
  const Config& config() const { return cfg_; }
  IStorage* stagingStorage() const { return stg_; }
 private:
  bool step_();
  bool fail_() { err_ = true; return false; }
  IStorage* stg_; uint16_t fsv_;
  LogObserver* obs_[4]; size_t nobs_;
  int st_; size_t need_, have_;
  uint8_t buf_[CFG_SLOT_SIZE];
  Crc32 crc_, recordsCrc_; Config cfg_; uint32_t cfgSeq_, nrec_, left_, lastSeq_;
  bool prevMore_, ok_, err_;
};
// A backup that was never finished, or hit a write error, can never be acknowledged.
inline bool ackMatches(const BackupWriter& w, uint32_t crc, uint32_t len) { return w.finished() && w.length() == len && w.crc() == crc; }
// Year rollover: setup code, typed confirmation, and a BACKUP_ACK for the current last seq.
inline bool rolloverAllowed(uint32_t ackedSeq, uint32_t lastSeq, bool typedConfirm, bool setupCodeOk) {
  return typedConfirm && setupCodeOk && lastSeq > 0 && ackedSeq == lastSeq;
}
}
