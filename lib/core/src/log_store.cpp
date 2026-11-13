#include "log_store.h"
namespace att {
void LogStore::track(const Event& e) {
  if (e.type == EV_PAYLOAD) return;
  if (e.type == EV_CLOCK_SET) {
    if (!flagMore(e.flags)) baseline_ = e.epoch;
    return;
  }
  if (e.epoch > baseline_) baseline_ = e.epoch;
}
void LogStore::notifyRecord(const Event& e) {
  for (size_t i = 0; i < nobs_; i++) obs_[i]->onRecord(e);
}
void LogStore::notifyReset() {
  for (size_t i = 0; i < nobs_; i++) obs_[i]->onReset();
}
bool LogStore::hasObserver(const LogObserver* observer) const {
  if (!observer) return false;
  for (size_t i = 0; i < nobs_; ++i)
    if (obs_[i] == observer) return true;
  return false;
}
bool LogStore::attach(LogObserver* o) {
  if (!o || nobs_ >= MAX_OBSERVERS) return false;
  for (size_t i = 0; i < nobs_; i++) if (obs_[i] == o) return false;   // double registration = double events
  obs_[nobs_++] = o;
  o->onReset();
  if (faulted_) return true;
  Event e;
  for (size_t i = 0; i < count_; i++) {
    if (!at(i, e)) {                                  // failed replay: do not leave a half-built observer registered
      obs_[--nobs_] = nullptr;
      o->onReset();
      return false;
    }
    o->onRecord(e);
  }
  return true;
}
OpenInfo LogStore::open() {
  OpenInfo r = {false, 0, 0, 0, 0, 0, -1, 0};
  faulted_ = true;
  if (firstSeq_ == 0 || openGeneration_ == ~(uint64_t)0) return r;
  r.generation = ++openGeneration_;
  count_ = 0; nextSeq_ = firstSeq_; baseline_ = 0;
  notifyReset();

  // Startup is inspection only. A malformed EOF record is not proof of a torn
  // append: it may be damage to previously committed attendance. Never truncate
  // bytes or replay a valid-looking prefix as healthy when any suffix is ambiguous.
  size_t size = 0;
  if (!s_.sizeChecked(size)) return r;
  const size_t n = size / REC_SIZE;
  if ((uint64_t)n > (uint64_t)UINT32_MAX - firstSeq_) return r;
  size_t good = 0;
  size_t firstBad = n;
  uint8_t buf[REC_SIZE];
  Event e;
  for (size_t i = 0; i < n; ++i) {
    if (!s_.read(i * REC_SIZE, buf, REC_SIZE)) return r;
    const bool valid = decode(buf, e) &&
        e.seq == firstSeq_ + (uint32_t)i;
    if (firstBad == n) {
      if (valid) {
        good = i + 1;
      } else {
        firstBad = i;
        r.corruptAt = (int32_t)(i * REC_SIZE);
      }
    } else if (decode(buf, e)) {
      // A decodable record after the first bad slot proves interior corruption.
      // Do not guess which later records or groups remain authoritative.
      return r;
    }
  }

  r.tornBytes = (uint32_t)(size - good * REC_SIZE);
  size_t completePrefix = good;
  while (completePrefix > 0) {
    if (!s_.read((completePrefix - 1) * REC_SIZE, buf, REC_SIZE) ||
        !decode(buf, e) ||
        e.seq != firstSeq_ + (uint32_t)(completePrefix - 1)) return r;
    if (flagMore(e.flags)) {
      --completePrefix;
    } else {
      break;
    }
  }
  r.droppedGroup = (uint32_t)(good - completePrefix);

  // Any invalid record, partial record, or unfinished commit group requires
  // service/recovery. Preserve every byte and keep all observers reset/unreplayed.
  if (r.corruptAt >= 0 || r.tornBytes != 0 || r.droppedGroup != 0) return r;

  for (size_t i = 0; i < good; ++i) {
    if (!s_.read(i * REC_SIZE, buf, REC_SIZE) || !decode(buf, e) ||
        e.seq != firstSeq_ + (uint32_t)i) {
      // A second-pass I/O error or storage change can occur after some records
      // reached observers. Roll every projection back before exposing failure.
      count_ = 0;
      nextSeq_ = firstSeq_;
      baseline_ = 0;
      notifyReset();
      return r;
    }
    track(e);
    notifyRecord(e);
  }
  count_ = good;
  nextSeq_ = firstSeq_ + (uint32_t)good;
  faulted_ = false;
  r.ok = true;
  r.records = (uint32_t)good;
  r.lastSeq = nextSeq_ - 1;
  r.baselineEpoch = baseline_;
  return r;
}
bool LogStore::append(const Event* evs, size_t n) {
  if (faulted_ || n == 0 || n > MAX_GROUP || nextSeq_ == 0 ||
      (uint64_t)n > (uint64_t)UINT32_MAX - nextSeq_) return false;
  uint8_t buf[MAX_GROUP * REC_SIZE];
  for (size_t i = 0; i < n; i++) {
    Event e = evs[i];
    e.seq = nextSeq_ + (uint32_t)i;
    e.flags = (uint8_t)((e.flags & (uint8_t)~FL_MORE) | (i + 1 < n ? FL_MORE : 0));
    encode(e, buf + i * REC_SIZE);
  }
  if (!s_.append(buf, n * REC_SIZE)) { faulted_ = true; return false; }
  for (size_t i = 0; i < n; i++) {
    Event e;
    decode(buf + i * REC_SIZE, e);
    track(e);
    notifyRecord(e);
  }
  count_ += n; nextSeq_ += (uint32_t)n;
  return true;
}
bool LogStore::at(size_t i, Event& e) {
  uint8_t buf[REC_SIZE];
  if (i >= count_ || !s_.read(i * REC_SIZE, buf, REC_SIZE)) return false;
  return decode(buf, e);
}
bool gapSpanEncodable(uint32_t from, uint32_t to) {
  if (to < from) return false;
  const uint64_t seconds = (uint64_t)to - (uint64_t)from;
  const uint64_t minutes = seconds / 60u + (seconds % 60u != 0u ? 1u : 0u);
  const uint64_t roundedEnd = (uint64_t)from + minutes * 60u;
  return roundedEnd <= UINT32_MAX;
}
size_t makeGapEvents(uint32_t from, uint32_t to, uint16_t cause, Event* out, size_t maxOut, uint32_t* next) {
  size_t n = 0;
  uint32_t cur = from;
  while (cur < to && n < maxOut) {
    const uint32_t remaining = to - cur;  // safe because cur < to
    uint32_t mins = remaining / 60u + (remaining % 60u != 0u ? 1u : 0u);
    if (mins > 65535u) mins = 65535u;

    // EV_GAP stores whole minutes, but its epoch and derived end are uint32.
    // Never round a final chunk beyond UINT32_MAX; return the representable
    // prefix through *next and let the caller reject an unrepresentable tail.
    const uint32_t minutesThatFit = (UINT32_MAX - cur) / 60u;
    if (mins > minutesThatFit) mins = minutesThatFit;
    if (mins == 0) break;  // no positive whole-minute chunk fits in the epoch domain

    out[n++] = Event{EV_GAP, 0, cause, cur, 0, (uint16_t)mins};
    cur += mins * 60u;     // bounded above by minutesThatFit; cannot wrap
  }
  if (next) *next = cur;
  return n;
}
bool appendGaps(LogStore& log, uint32_t from, uint32_t to, uint16_t cause) {
  if (!gapSpanEncodable(from, to)) return false;
  uint32_t cur = from;
  while (cur < to) {
    Event ev[MAX_GROUP];
    uint32_t next = cur;
    size_t n = makeGapEvents(cur, to, cause, ev, MAX_GROUP, &next);
    if (n == 0 || next <= cur || !log.append(ev, n)) return false;
    cur = next;
  }
  return true;
}
}
