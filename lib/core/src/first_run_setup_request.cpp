#include "first_run_setup_request.h"

#include <string.h>

namespace att {
namespace {

void wipe(void* value, size_t length) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(value);
  while (length--) *p++ = 0;
}

class JsonReader {
 public:
  JsonReader(const char* data, size_t length)
      : data_(data), length_(length), pos_(0) {}

  bool decode(FirstRunSetupRequest& request) {
    if (data_ == nullptr || length_ == 0 ||
        length_ > FIRST_RUN_SETUP_REQUEST_MAX_BYTES) return false;

    request.config = defaultConfig();
    skipSpace_();
    if (!take_('{')) return false;

    uint8_t seen = 0;
    bool first = true;
    while (true) {
      skipSpace_();
      if (peek_('}')) break;
      if (!first) {
        if (!take_(',')) return false;
        skipSpace_();
        if (peek_('}')) return false;
      }
      first = false;

      char key[32] = {};
      if (!string_(key, sizeof key) || !take_(':')) return false;

      uint8_t bit = 0;
      if (strcmp(key, "setup_code") == 0) {
        bit = 0x01;
        if ((seen & bit) || !sixDigits_(request.setupCode)) return false;
      } else if (strcmp(key, "initial_pin") == 0) {
        bit = 0x02;
        if ((seen & bit) || !sixDigits_(request.initialPin)) return false;
      } else if (strcmp(key, "config") == 0) {
        bit = 0x04;
        if ((seen & bit) || !config_(request.config)) return false;
      } else {
        return false;
      }
      seen = static_cast<uint8_t>(seen | bit);
      skipSpace_();
      if (peek_('}')) break;
    }

    if (!take_('}') || seen != 0x07) return false;
    skipSpace_();
    return pos_ == length_;
  }

 private:
  void skipSpace_() {
    while (pos_ < length_) {
      const char c = data_[pos_];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
      ++pos_;
    }
  }

  bool peek_(char c) {
    skipSpace_();
    return pos_ < length_ && data_[pos_] == c;
  }

  bool take_(char c) {
    skipSpace_();
    if (pos_ >= length_ || data_[pos_] != c) return false;
    ++pos_;
    return true;
  }

  bool string_(char* out, size_t capacity) {
    skipSpace_();
    if (out == nullptr || capacity == 0 || pos_ >= length_ ||
        data_[pos_++] != '"') return false;

    size_t used = 0;
    while (pos_ < length_) {
      const uint8_t c = static_cast<uint8_t>(data_[pos_++]);
      if (c == '"') {
        out[used] = '\0';
        return true;
      }
      // The schema contains only ASCII keys and digit-only credentials.
      // Reject escapes and controls instead of implementing partial Unicode.
      if (c < 0x20 || c > 0x7E || c == '\\' || used + 1 >= capacity)
        return false;
      out[used++] = static_cast<char>(c);
    }
    return false;
  }

  bool sixDigits_(char out[7]) {
    size_t length = 0;
    if (!string_(out, 7)) return false;
    length = strlen(out);
    if (length != 6) return false;
    for (size_t i = 0; i < 6; ++i)
      if (out[i] < '0' || out[i] > '9') return false;
    return true;
  }

  bool integer_(int64_t minimum, int64_t maximum, int64_t& out) {
    skipSpace_();
    if (pos_ >= length_) return false;

    bool negative = false;
    if (data_[pos_] == '-') {
      negative = true;
      ++pos_;
    }
    if (pos_ >= length_ || data_[pos_] < '0' || data_[pos_] > '9')
      return false;
    if (data_[pos_] == '0' && pos_ + 1 < length_ &&
        data_[pos_ + 1] >= '0' && data_[pos_ + 1] <= '9') return false;

    const uint64_t limit = negative
        ? static_cast<uint64_t>(-(minimum + 1)) + 1u
        : static_cast<uint64_t>(maximum);
    uint64_t magnitude = 0;
    while (pos_ < length_ && data_[pos_] >= '0' && data_[pos_] <= '9') {
      const uint8_t digit = static_cast<uint8_t>(data_[pos_] - '0');
      if (digit > limit || magnitude > (limit - digit) / 10u)
        return false;
      magnitude = magnitude * 10u + digit;
      ++pos_;
    }

    const int64_t value = negative
        ? -static_cast<int64_t>(magnitude)
        : static_cast<int64_t>(magnitude);
    if (value < minimum || value > maximum) return false;
    out = value;
    return true;
  }

  bool boolean_(bool& out) {
    skipSpace_();
    if (pos_ + 4 <= length_ &&
        memcmp(data_ + pos_, "true", 4) == 0) {
      pos_ += 4;
      out = true;
      return true;
    }
    if (pos_ + 5 <= length_ &&
        memcmp(data_ + pos_, "false", 5) == 0) {
      pos_ += 5;
      out = false;
      return true;
    }
    return false;
  }

  bool timetable_(Timetable& timetable) {
    if (!take_('[')) return false;
    for (size_t day = 0; day < 5; ++day) {
      if (day != 0 && !take_(',')) return false;
      if (!take_('[')) return false;
      for (size_t session = 0; session < 2; ++session) {
        if (session != 0 && !take_(',')) return false;
        if (!take_('[')) return false;

        int64_t start = 0, end = 0;
        bool enabled = false;
        if (!integer_(0, 1439, start) || !take_(',') ||
            !integer_(0, 1439, end) || !take_(',') ||
            !boolean_(enabled) || !take_(']')) return false;

        timetable.day[day][session] = Slot{
            static_cast<uint16_t>(start), static_cast<uint16_t>(end),
            enabled};
      }
      if (!take_(']')) return false;
    }
    return take_(']');
  }

  bool config_(Config& config) {
    if (!take_('{')) return false;
    uint16_t seen = 0;
    bool first = true;
    while (true) {
      skipSpace_();
      if (peek_('}')) break;
      if (!first) {
        if (!take_(',')) return false;
        skipSpace_();
        if (peek_('}')) return false;
      }
      first = false;

      char key[32] = {};
      int64_t value = 0;
      if (!string_(key, sizeof key) || !take_(':')) return false;
      uint16_t bit = 0;

      if (strcmp(key, "utc_offset_min") == 0) {
        bit = 0x0001;
        if (!integer_(-720, 840, value)) return false;
        config.params.utcOffsetMin = static_cast<int16_t>(value);
      } else if (strcmp(key, "late_min") == 0) {
        bit = 0x0002;
        if (!integer_(0, 240, value)) return false;
        config.params.lateMin = static_cast<uint16_t>(value);
      } else if (strcmp(key, "scan_lead_min") == 0) {
        bit = 0x0004;
        if (!integer_(0, 60, value)) return false;
        config.params.scanLeadMin = static_cast<uint16_t>(value);
      } else if (strcmp(key, "term_start_day") == 0) {
        bit = 0x0008;
        if (!integer_(0, MAX_TERM_DAY, value)) return false;
        config.termStart = static_cast<int32_t>(value);
      } else if (strcmp(key, "term_end_day") == 0) {
        bit = 0x0010;
        if (!integer_(0, MAX_TERM_DAY, value)) return false;
        config.termEnd = static_cast<int32_t>(value);
      } else if (strcmp(key, "auto_sync") == 0) {
        bit = 0x0020;
        if (!integer_(0, 1, value)) return false;
        config.autoSync = static_cast<uint8_t>(value);
      } else if (strcmp(key, "sound_mode") == 0) {
        bit = 0x0040;
        if (!integer_(0, 2, value)) return false;
        config.soundMode = static_cast<uint8_t>(value);
      } else if (strcmp(key, "timetable") == 0) {
        bit = 0x0080;
        if (!timetable_(config.tt)) return false;
      } else {
        return false;
      }

      if ((seen & bit) != 0) return false;
      seen = static_cast<uint16_t>(seen | bit);
      skipSpace_();
      if (peek_('}')) break;
    }

    if (!take_('}') || seen != 0x00FF) return false;
    return config.termEnd >= config.termStart;
  }

  const char* data_;
  size_t length_;
  size_t pos_;
};

}  // namespace

FirstRunSetupBodyBuffer::FirstRunSetupBodyBuffer()
    : bytes_{0}, bodyLength_(0), expectedLength_(0),
      receiving_(false), ready_(false) {}

FirstRunSetupBodyBuffer::~FirstRunSetupBodyBuffer() {
  clear();
}

void FirstRunSetupBodyBuffer::clear() {
  wipe(bytes_, sizeof bytes_);
  bodyLength_ = 0;
  expectedLength_ = 0;
  receiving_ = false;
  ready_ = false;
}

void FirstRunSetupBodyBuffer::abort() {
  clear();
}

FirstRunSetupBodyCode FirstRunSetupBodyBuffer::begin(size_t expectedLength) {
  clear();
  if (expectedLength == 0) return FRSB_INVALID_LENGTH;
  if (expectedLength > FIRST_RUN_SETUP_REQUEST_MAX_BYTES)
    return FRSB_TOO_LARGE;
  expectedLength_ = expectedLength;
  receiving_ = true;
  return FRSB_OK;
}

FirstRunSetupBodyCode FirstRunSetupBodyBuffer::append(
    const uint8_t* chunk, size_t currentSize, size_t totalSize) {
  if (!receiving_) {
    clear();
    return FRSB_BAD_STATE;
  }
  if (chunk == nullptr || currentSize == 0) {
    clear();
    return FRSB_INVALID_CHUNK;
  }
  if (bodyLength_ > FIRST_RUN_SETUP_REQUEST_MAX_BYTES ||
      currentSize > FIRST_RUN_SETUP_REQUEST_MAX_BYTES - bodyLength_) {
    clear();
    return FRSB_TOO_LARGE;
  }

  const size_t nextLength = bodyLength_ + currentSize;
  if (totalSize != nextLength) {
    clear();
    return FRSB_INVALID_CHUNK;
  }
  if (nextLength > expectedLength_) {
    clear();
    return FRSB_TOO_LARGE;
  }

  memcpy(bytes_ + bodyLength_, chunk, currentSize);
  bodyLength_ = nextLength;
  return FRSB_OK;
}

FirstRunSetupBodyCode FirstRunSetupBodyBuffer::finish() {
  if (!receiving_) {
    clear();
    return FRSB_BAD_STATE;
  }
  if (bodyLength_ != expectedLength_) {
    clear();
    return FRSB_INCOMPLETE;
  }
  bytes_[bodyLength_] = '\0';
  receiving_ = false;
  ready_ = true;
  return FRSB_OK;
}

FirstRunSetupRequestCode decodeFirstRunSetupRequest(
    const char* json, size_t length, FirstRunSetupRequest& out) {
  if (json == nullptr || length == 0 ||
      length > FIRST_RUN_SETUP_REQUEST_MAX_BYTES)
    return FRSR_INVALID_REQUEST;

  FirstRunSetupRequest candidate;
  memset(&candidate, 0, sizeof candidate);
  JsonReader reader(json, length);
  if (!reader.decode(candidate)) {
    clearFirstRunSetupRequest(candidate);
    return FRSR_INVALID_REQUEST;
  }

  out = candidate;
  clearFirstRunSetupRequest(candidate);
  return FRSR_OK;
}

void clearFirstRunSetupRequest(FirstRunSetupRequest& request) {
  wipe(&request, sizeof request);
}

}  // namespace att
