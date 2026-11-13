#include "log_index.h"
namespace att {
void LogIndex::reset_() { nGaps_ = 0; nCloses_ = 0; gapOverflow_ = false; closeOverflow_ = false; }
void LogIndex::onRecord(const Event& e) {
  if (e.type == EV_GAP) {
    const uint64_t end64 = (uint64_t)e.epoch + (uint64_t)e.b * 60u;
    if (e.b == 0 || end64 > UINT32_MAX) {
      gapOverflow_ = true;                 // malformed span: never let wraparound hide a coverage gap
      return;
    }
    uint32_t from = e.epoch;
    uint32_t to = (uint32_t)end64;
    // This index answers only interval-overlap questions. Keep a sorted union
    // of half-open spans so retries/chunks that overlap or touch use one slot.
    size_t first = 0;
    while (first < nGaps_ && gaps_[first].to < from) ++first;
    size_t last = first;
    while (last < nGaps_ && gaps_[last].from <= to) {
      if (gaps_[last].from < from) from = gaps_[last].from;
      if (gaps_[last].to > to) to = gaps_[last].to;
      ++last;
    }
    if (last > first) {
      // Replace the first match with the expanded union and remove any other matches.
      gaps_[first] = Span{from, to};
      const size_t removed = last - first - 1;
      for (size_t i = last; i < nGaps_; ++i) gaps_[i - removed] = gaps_[i];
      nGaps_ -= removed;
    } else if (nGaps_ < MAX_GAPS) {
      for (size_t i = nGaps_; i > first; --i) gaps_[i] = gaps_[i - 1];
      gaps_[first] = Span{from, to};
      ++nGaps_;
    } else {
      gapOverflow_ = true;
    }
    return;
  }
  if (e.type == EV_CLOSE) {
    int32_t day = (int32_t)e.b;
    uint8_t s = flagSession(e.flags);
    if (closed(day, s)) return;              // first CLOSE wins, same as the replay
    if (nCloses_ < MAX_CLOSES) {
      closes_[nCloses_].day = day;
      closes_[nCloses_].session = s;
      nCloses_++;
    } else {
      closeOverflow_ = true;
    }
  }
}
bool LogIndex::closed(int32_t day, uint8_t s) const {
  for (size_t i = 0; i < nCloses_; i++)
    if (closes_[i].day == day && closes_[i].session == s) return true;
  return false;
}
bool LogIndex::gapOverlaps(uint32_t ws, uint32_t we) const {
  for (size_t i = 0; i < nGaps_; i++)
    if (gaps_[i].from < we && gaps_[i].to > ws) return true;
  return false;
}
}
