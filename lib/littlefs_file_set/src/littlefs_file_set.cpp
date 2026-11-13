#include "littlefs_file_set.h"
#include "event_group_audit.h"
#include "log_store.h"
#include "roster_snapshot.h"
#if !defined(ESP_PLATFORM)
#include "littlefs_file_set_test.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits>
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace att {
namespace {

bool parseMonthName(const char* name, uint32_t& key) {
  if (!name || strlen(name) != 10 || memcmp(name + 6, ".bin", 4) != 0)
    return false;
  uint32_t year = 0;
  uint32_t month = 0;
  for (size_t i = 0; i < 6; ++i) {
    if (name[i] < '0' || name[i] > '9') return false;
    const uint32_t digit = (uint32_t)(name[i] - '0');
    if (i < 4) year = year * 10u + digit;
    else month = month * 10u + digit;
  }
  if (year == 0 || year > 9999 || month < 1 || month > 12) return false;
  key = year * 100u + month;
  return true;
}

bool validMonthKey(uint32_t key) {
  const uint32_t year = key / 100u;
  const uint32_t month = key % 100u;
  return year >= 1 && year <= 9999 && month >= 1 && month <= 12;
}

bool fitsOffT(size_t value) {
  return (uint64_t)value <= (uint64_t)std::numeric_limits<off_t>::max();
}

#if defined(ESP_PLATFORM)
bool closeReadOnlyDescriptor(int fd) { return close(fd) == 0; }
#else
bool closeReadOnlyDescriptor(
    int fd, const LittleFsFileSetTestOps* testOps) {
  return (testOps && testOps->closeFn
              ? testOps->closeFn(testOps->context, fd)
              : close(fd)) == 0;
}
#endif

bool statFileSetEntry(const char* path, struct stat& st
#if !defined(ESP_PLATFORM)
                      , const LittleFsFileSetTestOps* testOps = nullptr
#endif
                      ) {
#if defined(ESP_PLATFORM)
  // ESP-IDF's pinned newlib exposes stat(), not lstat(). The mounted
  // LittleFS format has only regular-file and directory entry types; it
  // does not represent symbolic links. Keep no-follow checks on POSIX hosts.
  return stat(path, &st) == 0;
#else
  if (testOps && testOps->statFn)
    return testOps->statFn(testOps->context, path, &st) == 0;
  return lstat(path, &st) == 0;
#endif
}

bool statRegularFile(const char* path, size_t& size
#if !defined(ESP_PLATFORM)
                     , const LittleFsFileSetTestOps* testOps = nullptr
#endif
                     ) {
  struct stat st;
#if defined(ESP_PLATFORM)
  if (!statFileSetEntry(path, st)) return false;
#else
  if (!statFileSetEntry(path, st, testOps)) return false;
#endif
  if (!S_ISREG(st.st_mode) || st.st_size < 0) return false;
  const uint64_t bytes = (uint64_t)st.st_size;
  if (bytes > (uint64_t)SIZE_MAX) return false;
  size = (size_t)bytes;
  return true;
}

#if !defined(ESP_PLATFORM)
int openReadOnlyFile(const char* path, const LittleFsFileSetTestOps* testOps) {
  if (testOps && testOps->openFn)
    return testOps->openFn(testOps->context, path, O_RDONLY, 0);
  return open(path, O_RDONLY);
}

int fstatReadOnlyFile(int fd, struct stat& st,
                      const LittleFsFileSetTestOps* testOps) {
  if (testOps && testOps->fstatFn)
    return testOps->fstatFn(testOps->context, fd, &st);
  return fstat(fd, &st);
}

DIR* openReadOnlyDirectory(const char* path,
                           const LittleFsFileSetTestOps* testOps) {
  if (testOps && testOps->opendirFn)
    return testOps->opendirFn(testOps->context, path);
  return opendir(path);
}

struct dirent* readDirectoryEntry(DIR* dir,
                                  const LittleFsFileSetTestOps* testOps) {
  if (testOps && testOps->readdirFn)
    return testOps->readdirFn(testOps->context, dir);
  return readdir(dir);
}

int closeReadOnlyDirectory(DIR* dir,
                           const LittleFsFileSetTestOps* testOps) {
  if (testOps && testOps->closedirFn)
    return testOps->closedirFn(testOps->context, dir);
  return closedir(dir);
}
#endif

class RosterAuditScratch {
 public:
  RosterAuditScratch(size_t fileCount, int16_t utcOffsetMin)
      : required_(fileCount != 0),
        roster_(required_ ? new (std::nothrow) RosterStore(utcOffsetMin) : nullptr),
        transition_(required_ ? new (std::nothrow) RosterTransitionAudit()
                              : nullptr),
        valid_(true) {}

  ~RosterAuditScratch() {
    delete transition_;
    delete roster_;
  }

  RosterAuditScratch(const RosterAuditScratch&) = delete;
  RosterAuditScratch& operator=(const RosterAuditScratch&) = delete;

  void consume(const Event& event, int16_t utcOffsetMin) {
    if (!required_) return;
    if (!roster_ || !transition_) {
      valid_ = false;
      return;
    }
    roster_->onRecord(event);
    if (event.type == EV_ROSTER &&
        !transition_->consume(event, utcOffsetMin))
      valid_ = false;
  }

  bool valid() const {
    return valid_ && (!required_ || (roster_ && transition_));
  }

  bool complete() const {
    return valid() && (!required_ ||
        (roster_->historyComplete() && transition_->matches(*roster_)));
  }

 private:
  bool required_;
  RosterStore* roster_;
  RosterTransitionAudit* transition_;
  bool valid_;
};

// LogStore calls onReset() whenever an observer attaches. The adapter's
// deliberate no-op preserves roster history across month-scoped LogStores.
class RosterStreamObserver final : public LogObserver {
 public:
  RosterStreamObserver(RosterAuditScratch& scratch, int16_t utcOffsetMin)
      : scratch_(scratch), utcOffsetMin_(utcOffsetMin) {}
  void onReset() override {}
  void onRecord(const Event& event) override {
    scratch_.consume(event, utcOffsetMin_);
  }

 private:
  RosterAuditScratch& scratch_;
  int16_t utcOffsetMin_;
};

class ReadOnlyFileStorage final : public IStorage {
 public:
#if defined(ESP_PLATFORM)
  ReadOnlyFileStorage(int fd, size_t size) : fd_(fd), size_(size) {}
#else
  ReadOnlyFileStorage(int fd, size_t size,
                      const LittleFsFileSetTestOps* testOps)
      : fd_(fd), size_(size), testOps_(testOps) {}
#endif

  bool sizeChecked(size_t& out) override {
    out = 0;
    if (fd_ < 0) return false;
    out = size_;
    return true;
  }

  bool read(size_t offset, uint8_t* out, size_t count) override {
    if (fd_ < 0 || offset > size_ || count > size_ - offset ||
        (count != 0 && !out) || !fitsOffT(offset)) return false;
    size_t done = 0;
    while (done < count) {
      const size_t chunk = count - done > 256 ? 256 : count - done;
      if (done > SIZE_MAX - offset || !fitsOffT(offset + done)) return false;
#if defined(ESP_PLATFORM)
      const ssize_t got = pread(fd_, out + done, chunk,
                                (off_t)(offset + done));
#else
      const ssize_t got = testOps_ && testOps_->preadFn
          ? testOps_->preadFn(testOps_->context, fd_, out + done, chunk,
                              (off_t)(offset + done))
          : pread(fd_, out + done, chunk, (off_t)(offset + done));
#endif
      if (got < 0 && errno == EINTR) continue;
      if (got <= 0) return false;
      done += (size_t)got;
    }
    return done == count;
  }

  bool append(const uint8_t*, size_t) override { return false; }
  bool truncate(size_t) override { return false; }

 private:
  int fd_;
  size_t size_;
#if !defined(ESP_PLATFORM)
  const LittleFsFileSetTestOps* testOps_;
#endif
};

}  // namespace

#if defined(ESP_PLATFORM)
LittleFsFileSet::LittleFsFileSet(const char* mountpoint)
    : mountpoint_{0}, valid_(false) {
#else
LittleFsFileSet::LittleFsFileSet(const char* mountpoint)
    : LittleFsFileSet(mountpoint, nullptr) {}

LittleFsFileSet::LittleFsFileSet(
    const char* mountpoint, const LittleFsFileSetTestOps* testOps)
    : mountpoint_{0}, valid_(false), testOps_(testOps) {
#endif
  if (!mountpoint || mountpoint[0] != '/') return;
  const size_t length = strlen(mountpoint);
  if (length == 0 || length >= sizeof mountpoint_) return;
  if (length > 1 && mountpoint[length - 1] == '/') return;

  size_t segmentLength = 0;
  for (size_t i = 1; i <= length; ++i) {
    const char ch = i == length ? '/' : mountpoint[i];
    if (ch == '/') {
      if (segmentLength == 0 && i != length) return;
      if (segmentLength == 1 && mountpoint[i - 1] == '.') return;
      if (segmentLength == 2 && mountpoint[i - 1] == '.' &&
          mountpoint[i - 2] == '.') return;
      segmentLength = 0;
      continue;
    }
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      return;
    ++segmentLength;
  }
  memcpy(mountpoint_, mountpoint, length + 1);
  valid_ = true;
}

bool LittleFsFileSet::makePath_(const char* suffix, char* out,
                                size_t capacity) const {
  if (!valid_ || !suffix || suffix[0] != '/' || !out || capacity == 0)
    return false;
  const int n = mountpoint_[1] == '\0'
      ? snprintf(out, capacity, "%s", suffix)
      : snprintf(out, capacity, "%s%s", mountpoint_, suffix);
  return n > 0 && (size_t)n < capacity;
}

bool LittleFsFileSet::monthPath_(uint32_t key, char* out,
                                 size_t capacity) const {
  if (!validMonthKey(key)) return false;
  char suffix[20];
  const int n = snprintf(suffix, sizeof suffix, "/log/%04lu%02lu.bin",
                         (unsigned long)(key / 100u),
                         (unsigned long)(key % 100u));
  return n > 0 && (size_t)n < sizeof suffix &&
         makePath_(suffix, out, capacity);
}

bool LittleFsFileSet::ensureLogDirectory_(bool create) const {
  char root[PATH_CAP], logDir[PATH_CAP];
  if (!makePath_("/", root, sizeof root) ||
      !makePath_("/log", logDir, sizeof logDir)) return false;

  if (root[1] != '\0') root[strlen(root) - 1] = '\0';
  struct stat st;
#if defined(ESP_PLATFORM)
  if (!statFileSetEntry(root, st) || !S_ISDIR(st.st_mode)) return false;
  if (statFileSetEntry(logDir, st)) return S_ISDIR(st.st_mode);
#else
  if (!statFileSetEntry(root, st, testOps_) || !S_ISDIR(st.st_mode))
    return false;
  if (statFileSetEntry(logDir, st, testOps_)) return S_ISDIR(st.st_mode);
#endif
  if (errno != ENOENT || !create) return false;

  if (mkdir(logDir, 0700) == 0) return true;
  if (errno != EEXIST) return false;
#if defined(ESP_PLATFORM)
  return statFileSetEntry(logDir, st) && S_ISDIR(st.st_mode);
#else
  return statFileSetEntry(logDir, st, testOps_) && S_ISDIR(st.st_mode);
#endif
}

bool LittleFsFileSet::scan_(size_t* count, uint32_t afterKey,
                            uint32_t* nextKey,
                            size_t fileSizeAlignment) const {
  if (count) *count = 0;
  if (nextKey) *nextKey = 0;

  // Inspect root and /log exactly once. A second stat after an arbitrary
  // metadata failure could turn a transient EIO into a false "empty" result.
  char root[PATH_CAP], logDir[PATH_CAP];
  if (!makePath_("/", root, sizeof root) ||
      !makePath_("/log", logDir, sizeof logDir))
    return false;
  if (root[1] != '\0') root[strlen(root) - 1] = '\0';
  struct stat st;
#if defined(ESP_PLATFORM)
  if (!statFileSetEntry(root, st) || !S_ISDIR(st.st_mode)) return false;
  if (!statFileSetEntry(logDir, st)) return errno == ENOENT;
#else
  if (!statFileSetEntry(root, st, testOps_) || !S_ISDIR(st.st_mode))
    return false;
  if (!statFileSetEntry(logDir, st, testOps_)) return errno == ENOENT;
#endif
  if (!S_ISDIR(st.st_mode)) return false;
#if defined(ESP_PLATFORM)
  DIR* dir = opendir(logDir);
#else
  DIR* dir = openReadOnlyDirectory(logDir, testOps_);
#endif
  if (!dir) return false;

  size_t foundCount = 0;
  uint32_t smallest = 0;
  bool haveNext = false;
  bool ok = true;
  for (;;) {
    errno = 0;
#if defined(ESP_PLATFORM)
    struct dirent* entry = readdir(dir);
#else
    struct dirent* entry = readDirectoryEntry(dir, testOps_);
#endif
    if (!entry) {
      if (errno != 0) ok = false;
      break;
    }
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0) continue;

    uint32_t key = 0;
    if (!parseMonthName(entry->d_name, key)) { ok = false; break; }
    char suffix[24], path[PATH_CAP];
    const int n = snprintf(suffix, sizeof suffix, "/log/%s", entry->d_name);
    size_t ignoredSize = 0;
    if (n <= 0 || (size_t)n >= sizeof suffix ||
        !makePath_(suffix, path, sizeof path) ||
#if defined(ESP_PLATFORM)
        !statRegularFile(path, ignoredSize) ||
#else
        !statRegularFile(path, ignoredSize, testOps_) ||
#endif
        (fileSizeAlignment != 0 &&
         (ignoredSize % fileSizeAlignment) != 0)) {
      ok = false;
      break;
    }
    if (foundCount == SIZE_MAX) { ok = false; break; }
    ++foundCount;
    if (nextKey && key > afterKey &&
        (!haveNext || key < smallest)) {
      smallest = key;
      haveNext = true;
    }
  }
#if defined(ESP_PLATFORM)
  if (closedir(dir) != 0) ok = false;
#else
  if (closeReadOnlyDirectory(dir, testOps_) != 0) ok = false;
#endif
  if (!ok) return false;

  if (count) *count = foundCount;
  if (nextKey && haveNext) *nextKey = smallest;
  return true;
}

bool LittleFsFileSet::countChecked(size_t& out) {
  out = 0;
  return scan_(&out, 0, nullptr);
}

bool LittleFsFileSet::countCheckedWithFileSizeAlignment(
    size_t alignment, size_t& out) {
  out = 0;
  if (alignment == 0) return false;
  return scan_(&out, 0, nullptr, alignment);
}

bool LittleFsFileSet::validateRecordFiles(size_t fileCount,
                                             int16_t utcOffsetMin) {
  if (!valid_ || utcOffsetMin < -720 || utcOffsetMin > 840) return false;

  // Confirm the root and log directory independently of the earlier count
  // pass. A missing log directory is valid only when the expected count is 0.
  char root[PATH_CAP], logDir[PATH_CAP];
  if (!makePath_("/", root, sizeof root) ||
      !makePath_("/log", logDir, sizeof logDir)) return false;
  if (root[1] != '\0') root[strlen(root) - 1] = '\0';

  struct stat rootStat, logStat;
#if defined(ESP_PLATFORM)
  if (!statFileSetEntry(root, rootStat) || !S_ISDIR(rootStat.st_mode))
    return false;
  if (!statFileSetEntry(logDir, logStat))
    return errno == ENOENT && fileCount == 0;
#else
  if (!statFileSetEntry(root, rootStat, testOps_) ||
      !S_ISDIR(rootStat.st_mode)) return false;
  if (!statFileSetEntry(logDir, logStat, testOps_))
    return errno == ENOENT && fileCount == 0;
#endif
  if (!S_ISDIR(logStat.st_mode)) return false;

  struct RecordFileInfo {
    uint32_t key;
    size_t size;
  };
  if (fileCount > SIZE_MAX / sizeof(RecordFileInfo)) return false;

#if defined(ESP_PLATFORM)
  DIR* dir = opendir(logDir);
#else
  DIR* dir = openReadOnlyDirectory(logDir, testOps_);
#endif
  if (!dir) return false;
  RecordFileInfo* entries = fileCount
      ? static_cast<RecordFileInfo*>(
            malloc(fileCount * sizeof(RecordFileInfo)))
      : nullptr;
  if (fileCount != 0 && !entries) {
#if defined(ESP_PLATFORM)
    (void)closedir(dir);
#else
    (void)closeReadOnlyDirectory(dir, testOps_);
#endif
    return false;
  }

  size_t found = 0;
  bool ok = true;
  for (;;) {
    errno = 0;
#if defined(ESP_PLATFORM)
    struct dirent* entry = readdir(dir);
#else
    struct dirent* entry = readDirectoryEntry(dir, testOps_);
#endif
    if (!entry) {
      if (errno != 0) ok = false;
      break;
    }
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0) continue;

    uint32_t key = 0;
    char path[PATH_CAP];
    size_t bytes = 0;
    const bool validName = parseMonthName(entry->d_name, key) &&
        monthPath_(key, path, sizeof path);
    if (!validName) {
      ok = false;
      break;
    }
#if defined(ESP_PLATFORM)
    const bool regularFile = statRegularFile(path, bytes);
#else
    const bool regularFile = statRegularFile(path, bytes, testOps_);
#endif
    if (!regularFile || (bytes % REC_SIZE) != 0 || found >= fileCount) {
      ok = false;
      break;
    }
    entries[found].key = key;
    entries[found].size = bytes;
    ++found;
  }
#if defined(ESP_PLATFORM)
  if (closedir(dir) != 0) ok = false;
#else
  if (closeReadOnlyDirectory(dir, testOps_) != 0) ok = false;
#endif
  if (!ok || found != fileCount) {
    free(entries);
    return false;
  }

  // Directory iteration order is unspecified. Validate the global sequence
  // in strict chronological month order using this one metadata snapshot.
  if (found > 1) {
    qsort(entries, found, sizeof(RecordFileInfo),
          [](const void* lhs, const void* rhs) -> int {
            const RecordFileInfo* a =
                static_cast<const RecordFileInfo*>(lhs);
            const RecordFileInfo* b =
                static_cast<const RecordFileInfo*>(rhs);
            return a->key < b->key ? -1 : (a->key > b->key ? 1 : 0);
          });
  }
  for (size_t i = 1; i < found; ++i) {
    if (entries[i - 1].key >= entries[i].key) {
      free(entries);
      return false;
    }
  }

  RosterAuditScratch rosterScratch(found, utcOffsetMin);
  if (!rosterScratch.valid()) {
    free(entries);
    return false;
  }
  RosterStreamObserver rosterObserver(rosterScratch, utcOffsetMin);

  uint32_t firstSeq = 1;
  for (size_t i = 0; i < found; ++i) {
    char path[PATH_CAP];
    if (!monthPath_(entries[i].key, path, sizeof path)) {
      free(entries);
      return false;
    }

#if defined(ESP_PLATFORM)
    const int fd = open(path, O_RDONLY);
#else
    const int fd = openReadOnlyFile(path, testOps_);
#endif
    if (fd < 0) {
      free(entries);
      return false;
    }

    struct stat openedStat;
#if defined(ESP_PLATFORM)
    const bool descriptorStatOk = fstat(fd, &openedStat) == 0;
#else
    const bool descriptorStatOk =
        fstatReadOnlyFile(fd, openedStat, testOps_) == 0;
#endif
    const bool descriptorMatches =
        descriptorStatOk && S_ISREG(openedStat.st_mode) &&
        openedStat.st_size >= 0 &&
        (uint64_t)openedStat.st_size <= (uint64_t)SIZE_MAX &&
        (size_t)openedStat.st_size == entries[i].size;
    if (!descriptorMatches) {
#if defined(ESP_PLATFORM)
      (void)closeReadOnlyDescriptor(fd);
#else
      (void)closeReadOnlyDescriptor(fd, testOps_);
#endif
      free(entries);
      return false;
    }

#if defined(ESP_PLATFORM)
    ReadOnlyFileStorage storage(fd, entries[i].size);
#else
    ReadOnlyFileStorage storage(fd, entries[i].size, testOps_);
#endif
    LogStore log(storage, firstSeq);
    const OpenInfo opened = log.open();
    EventGroupAudit audit(firstSeq);
    const bool auditReady = opened.ok && audit.attach(log) &&
        audit.readyFor(log, opened);
    const bool rosterAttached = auditReady && log.attach(&rosterObserver);
#if defined(ESP_PLATFORM)
    const bool closeOk = closeReadOnlyDescriptor(fd);
#else
    const bool closeOk = closeReadOnlyDescriptor(fd, testOps_);
#endif
    if (!opened.ok || !auditReady || !rosterAttached ||
        !rosterScratch.valid() || !closeOk || opened.lastSeq == UINT32_MAX) {
      free(entries);
      return false;
    }
    firstSeq = opened.lastSeq + 1;
  }

  const bool rosterComplete = rosterScratch.complete();
  free(entries);
  return rosterComplete;
}

bool LittleFsFileSet::monthKeyChecked(size_t index, uint32_t& out) {
  out = 0;
  size_t count = 0;
  if (!scan_(&count, 0, nullptr) || index >= count) return false;

  uint32_t previous = 0;
  for (size_t rank = 0; rank <= index; ++rank) {
    uint32_t next = 0;
    if (!scan_(nullptr, previous, &next) || next == 0) return false;
    previous = next;
  }
  out = previous;
  return true;
}

bool LittleFsFileSet::sizeAtKey_(uint32_t key, size_t& out) const {
  out = 0;
  char path[PATH_CAP];
  if (!monthPath_(key, path, sizeof path)) return false;
  return statRegularFile(path, out);
}

bool LittleFsFileSet::fileSizeChecked(size_t index, size_t& out) {
  out = 0;
  uint32_t key = 0;
  return monthKeyChecked(index, key) && sizeAtKey_(key, out);
}

bool LittleFsFileSet::readFile(size_t index, size_t offset, uint8_t* out,
                               size_t count) {
  uint32_t key = 0;
  size_t size = 0;
  if (!monthKeyChecked(index, key) || !sizeAtKey_(key, size) ||
      offset > size || count > size - offset || (count && !out))
    return false;
  if (count == 0) return true;
  if (!fitsOffT(offset)) return false;

  char path[PATH_CAP];
  if (!monthPath_(key, path, sizeof path)) return false;
#if defined(ESP_PLATFORM)
  const int fd = open(path, O_RDONLY);
#else
  const int fd = openReadOnlyFile(path, testOps_);
#endif
  if (fd < 0) return false;

  bool ok = true;
  size_t done = 0;
  while (done < count) {
    const size_t chunk = count - done > 256 ? 256 : count - done;
    if (done > SIZE_MAX - offset || !fitsOffT(offset + done)) {
      ok = false;
      break;
    }
#if defined(ESP_PLATFORM)
    const ssize_t got = pread(fd, out + done, chunk,
                              (off_t)(offset + done));
#else
    const ssize_t got = testOps_ && testOps_->preadFn
        ? testOps_->preadFn(testOps_->context, fd, out + done, chunk,
                            (off_t)(offset + done))
        : pread(fd, out + done, chunk, (off_t)(offset + done));
#endif
    if (got < 0 && errno == EINTR) continue;
    if (got <= 0) { ok = false; break; }
    done += (size_t)got;
  }
#if defined(ESP_PLATFORM)
  if (close(fd) != 0) ok = false;
#else
  if (!closeReadOnlyDescriptor(fd, testOps_)) ok = false;
#endif
  return ok && done == count;
}

bool LittleFsFileSet::appendFile(size_t index, const uint8_t* data,
                                 size_t count) {
  if (!data || count == 0 || (count % 16u) != 0) return false;

  uint32_t key = 0;
  size_t before = 0;
  if (!monthKeyChecked(index, key) || !sizeAtKey_(key, before) ||
      (before % 16u) != 0 || count > SIZE_MAX - before ||
      !fitsOffT(before + count))
    return false;

  char path[PATH_CAP];
  if (!monthPath_(key, path, sizeof path)) return false;
  const int fd = open(path, O_RDWR | O_APPEND);
  if (fd < 0) return false;

  bool ok = true;
  size_t current = 0;
  if (!statRegularFile(path, current) || current != before) ok = false;

  size_t written = 0;
  while (ok && written < count) {
    const size_t chunk = count - written > 256 ? 256 : count - written;
#if defined(ESP_PLATFORM)
    const ssize_t put = write(fd, data + written, chunk);
#else
    const ssize_t put = testOps_ && testOps_->writeFn
        ? testOps_->writeFn(testOps_->context, fd, data + written, chunk)
        : write(fd, data + written, chunk);
#endif
    if (put < 0 && errno == EINTR) continue;
    if (put <= 0) { ok = false; break; }
    written += (size_t)put;
  }

#if defined(ESP_PLATFORM)
  if (fsync(fd) != 0) ok = false;
#else
  if ((testOps_ && testOps_->fsyncFn
           ? testOps_->fsyncFn(testOps_->context, fd)
           : fsync(fd)) != 0)
    ok = false;
#endif

  size_t after = 0;
  if (!statRegularFile(path, after) || after != before + count)
    ok = false;

  uint8_t verify[64];
  size_t verified = 0;
  while (ok && verified < count) {
    const size_t chunk = count - verified > sizeof verify
        ? sizeof verify : count - verified;
    if (verified > SIZE_MAX - before || !fitsOffT(before + verified)) {
      ok = false;
      break;
    }
#if defined(ESP_PLATFORM)
    const ssize_t got = pread(fd, verify, chunk,
                              (off_t)(before + verified));
#else
    const ssize_t got = testOps_ && testOps_->preadFn
        ? testOps_->preadFn(testOps_->context, fd, verify, chunk,
                            (off_t)(before + verified))
        : pread(fd, verify, chunk, (off_t)(before + verified));
#endif
    if (got < 0 && errno == EINTR) continue;
    if (got <= 0 || (size_t)got != chunk ||
        memcmp(verify, data + verified, chunk) != 0) {
      ok = false;
      break;
    }
    verified += chunk;
  }

#if defined(ESP_PLATFORM)
  if (close(fd) != 0) ok = false;
#else
  if ((testOps_ && testOps_->closeFn
           ? testOps_->closeFn(testOps_->context, fd)
           : close(fd)) != 0)
    ok = false;
#endif
  return ok && written == count && verified == count;
}

bool LittleFsFileSet::addFile(uint32_t monthKey) {
  if (!validMonthKey(monthKey) || !ensureLogDirectory_(true)) return false;

  size_t count = 0;
  if (!countChecked(count)) return false;
  if (count > 0) {
    uint32_t newest = 0;
    if (!monthKeyChecked(count - 1, newest) || monthKey <= newest)
      return false;
  }

  char path[PATH_CAP];
  if (!monthPath_(monthKey, path, sizeof path)) return false;
#if defined(ESP_PLATFORM)
  const int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#else
  const int fd = testOps_ && testOps_->openFn
      ? testOps_->openFn(testOps_->context, path,
                         O_WRONLY | O_CREAT | O_EXCL, 0600)
      : open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
  if (fd < 0) return false;

#if defined(ESP_PLATFORM)
  bool ok = fsync(fd) == 0;
  if (close(fd) != 0) ok = false;
#else
  bool ok = (testOps_ && testOps_->fsyncFn
                 ? testOps_->fsyncFn(testOps_->context, fd)
                 : fsync(fd)) == 0;
  if ((testOps_ && testOps_->closeFn
           ? testOps_->closeFn(testOps_->context, fd)
           : close(fd)) != 0)
    ok = false;
#endif

  size_t size = 0;
  if (!sizeAtKey_(monthKey, size) || size != 0) ok = false;
  return ok;
}

bool LittleFsFileSet::truncateFile(size_t, size_t) { return false; }
bool LittleFsFileSet::quarantineCopy(size_t) { return false; }
bool LittleFsFileSet::quarantineFrom(size_t) { return false; }

}  // namespace att
