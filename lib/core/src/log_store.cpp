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
  OpenInfo r = {false, 0, 0, 0, 0, 0, -1};
  faulted_ = true; count_ = 0; nextSeq_ = 1; baseline_ = 0;
  notifyReset();
  size_t size = s_.size();
  size_t n = size / REC_SIZE;
  size_t good = 0, firstBad = n;
  uint8_t buf[REC_SIZE];
  Event e;
  for (size_t i = 0; i < n; i++) {
    if (!s_.read(i * REC_SIZE, buf, REC_SIZE)) return r;
    bool valid = decode(buf, e);
    if (firstBad == n) {
      if (valid && e.seq == i + 1) good = i + 1; else firstBad = i;
    } else if (valid) {
      r.corruptAt = (int32_t)(firstBad * REC_SIZE);
      return r;
    }
  }
  r.tornBytes = (uint32_t)(size - good * REC_SIZE);
  size_t keep = good;
  while (keep > 0) {
    if (!s_.read((keep - 1) * REC_SIZE, buf, REC_SIZE) || !decode(buf, e)) return r;
    if (flagMore(e.flags)) keep--; else break;
  }
  r.droppedGroup = (uint32_t)(good - keep);
  if (keep * REC_SIZE != size && !s_.truncate(keep * REC_SIZE)) return r;
  for (size_t i = 0; i < keep; i++) {
    if (!s_.read(i * REC_SIZE, buf, REC_SIZE) || !decode(buf, e)) return r;
    track(e);
    notifyRecord(e);
  }
  count_ = keep; nextSeq_ = (uint32_t)keep + 1; faulted_ = false;
  r.ok = true; r.records = (uint32_t)keep; r.lastSeq = nextSeq_ - 1; r.baselineEpoch = baseline_;
  return r;
}
bool LogStore::append(const Event* evs, size_t n) {
  if (faulted_ || n == 0 || n > MAX_GROUP) return false;
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
size_t makeGapEvents(uint32_t from, uint32_t to, uint16_t cause, Event* out, size_t maxOut, uint32_t* next) {
  size_t n = 0;
  uint32_t cur = from;
  while (cur < to && n < maxOut) {
    uint32_t mins = (to - cur + 59) / 60;
    if (mins > 65535) mins = 65535;
    out[n++] = Event{EV_GAP, 0, cause, cur, 0, (uint16_t)mins};
    cur += mins * 60;
  }
  if (next) *next = cur;
  return n;
}
bool appendGaps(LogStore& log, uint32_t from, uint32_t to, uint16_t cause) {
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
