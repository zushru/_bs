#include "log_index.h"
namespace att {
void LogIndex::reset_() { nGaps_ = 0; nCloses_ = 0; gapOverflow_ = false; closeOverflow_ = false; }
void LogIndex::onRecord(const Event& e) {
  if (e.type == EV_GAP) {
    if (nGaps_ < MAX_GAPS) {
      gaps_[nGaps_].from = e.epoch;
      gaps_[nGaps_].to = e.epoch + (uint32_t)e.b * 60u;
      nGaps_++;
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
