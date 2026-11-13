#include "littlefs_vfs_probe.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace att {
namespace {

const size_t kPayloadSize = 512;
const size_t kPathCapacity = 112;

bool isUnsupportedDirectorySyncError(int error) {
  if (error == EINVAL || error == EBADF || error == EISDIR ||
      error == ENOSYS)
    return true;
#ifdef ENOTSUP
  if (error == ENOTSUP) return true;
#endif
#ifdef EOPNOTSUPP
  if (error == EOPNOTSUPP) return true;
#endif
  return false;
}

LittleFsDirectorySyncResult syncDirectory(const char* path, int& error,
                                          bool& closeFailed) {
  error = 0;
  const int fd = open(path, O_RDONLY);
  if (fd < 0) {
    error = errno;
    return isUnsupportedDirectorySyncError(error) ? LFDS_UNSUPPORTED
                                                   : LFDS_FAILED;
  }

  const int syncResult = fsync(fd);
  const int syncError = syncResult == 0 ? 0 : errno;
  const int closeResult = close(fd);
  const int closeError = closeResult == 0 ? 0 : errno;

  if (closeResult != 0) {
    closeFailed = true;
    error = closeError;
    return LFDS_FAILED;
  }
  if (syncResult == 0) return LFDS_SUPPORTED;
  error = syncError;
  return isUnsupportedDirectorySyncError(error) ? LFDS_UNSUPPORTED
                                                 : LFDS_FAILED;
}

bool makePath(char* out, size_t capacity, const char* root,
              const char* suffix) {
  if (!out || capacity == 0 || !root || root[0] != '/' || !suffix)
    return false;
  const size_t rootLength = strlen(root);
  if (rootLength < 2 || rootLength >= capacity ||
      root[rootLength - 1] == '/')
    return false;
  const int written = snprintf(out, capacity, "%s/%s", root, suffix);
  return written > 0 && static_cast<size_t>(written) < capacity;
}

bool readExact(int fd, uint8_t* output, size_t count, size_t& done,
               int& error) {
  done = 0;
  error = 0;
  while (done < count) {
    const ssize_t received = pread(fd, output + done, count - done,
                                   static_cast<off_t>(done));
    if (received < 0 && errno == EINTR) continue;
    if (received <= 0) {
      error = received == 0 ? EIO : errno;
      return false;
    }
    done += static_cast<size_t>(received);
  }
  return true;
}

bool removeOwnedPath(const char* path, bool exists, int& error) {
  if (!exists) return true;
  if (unlink(path) == 0 || errno == ENOENT) return true;
  error = errno;
  return false;
}

}  // namespace

bool runLittleFsVfsProbe(const char* mountpoint, uint32_t runId,
                         LittleFsVfsProbeResult& result) {
  memset(&result, 0, sizeof result);
  if (!mountpoint || mountpoint[0] != '/' || strlen(mountpoint) >= 40 ||
      mountpoint[strlen(mountpoint) - 1] == '/') {
    result.firstError = EINVAL;
    return false;
  }

  char directory[kPathCapacity] = {};
  char temporary[kPathCapacity] = {};
  char finalPath[kPathCapacity] = {};
  char directoryName[32] = {};
  snprintf(directoryName, sizeof directoryName, ".att_c13_vfs_%08lx",
           static_cast<unsigned long>(runId));
  if (!makePath(directory, sizeof directory, mountpoint, directoryName) ||
      !makePath(temporary, sizeof temporary, directory, "write.tmp") ||
      !makePath(finalPath, sizeof finalPath, directory, "readback.bin")) {
    result.firstError = ENAMETOOLONG;
    return false;
  }

  if (mkdir(directory, 0700) != 0) {
    result.firstError = errno;
    return false;
  }
  result.directoryCreated = true;
  result.parentSyncAfterCreate =
      syncDirectory(mountpoint, result.parentSyncAfterCreateError,
                    result.directorySyncCloseFailed);
  if (result.parentSyncAfterCreate == LFDS_FAILED) {
    if (result.firstError == 0)
      result.firstError = result.parentSyncAfterCreateError;
  }

  uint8_t expected[kPayloadSize] = {};
  uint8_t actual[kPayloadSize] = {};
  for (size_t i = 0; i < sizeof expected; ++i) {
    const unsigned shift = static_cast<unsigned>((i & 3u) * 8u);
    expected[i] = static_cast<uint8_t>((runId >> shift) ^
                                       static_cast<uint32_t>(i * 37u + 0x5au));
  }

  bool temporaryExists = false;
  bool finalExists = false;
  bool operationsOk = result.parentSyncAfterCreate != LFDS_FAILED;
  int fd = open(temporary, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0) {
    result.writeError = errno;
    if (result.firstError == 0) result.firstError = errno;
    operationsOk = false;
  } else {
    temporaryExists = true;
    result.fileCreated = true;
  }

  if (fd >= 0) {
    while (result.bytesWritten < sizeof expected) {
      const ssize_t written = write(fd, expected + result.bytesWritten,
                                    sizeof expected - result.bytesWritten);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) {
        result.writeError = written == 0 ? EIO : errno;
        if (result.firstError == 0) result.firstError = result.writeError;
        operationsOk = false;
        break;
      }
      result.bytesWritten += static_cast<size_t>(written);
    }
    result.writeComplete = result.bytesWritten == sizeof expected;

    if (fsync(fd) == 0) {
      result.fileSyncComplete = true;
    } else {
      result.fileSyncError = errno;
      if (result.firstError == 0) result.firstError = errno;
      operationsOk = false;
    }

    struct stat fileStat;
    const int statResult = fstat(fd, &fileStat);
    if (statResult == 0 && S_ISREG(fileStat.st_mode) &&
        fileStat.st_size == static_cast<off_t>(sizeof expected)) {
      result.fileStatMatches = true;
    } else {
      result.fileStatError = statResult != 0 ? errno : EIO;
      if (result.firstError == 0) result.firstError = result.fileStatError;
      operationsOk = false;
    }

    int readError = 0;
    result.readbackMatches = readExact(fd, actual, sizeof actual,
                                       result.bytesRead, readError) &&
                             memcmp(expected, actual, sizeof expected) == 0;
    if (!result.readbackMatches) {
      result.readError = readError == 0 ? EILSEQ : readError;
      if (result.firstError == 0) result.firstError = result.readError;
      operationsOk = false;
    }

    const int closeResult = close(fd);
    const int closeError = closeResult == 0 ? 0 : errno;
    fd = -1;
    result.fileCloseComplete = closeResult == 0;
    result.fileCloseError = closeError;
    if (!result.fileCloseComplete) {
      if (result.firstError == 0) result.firstError = closeError;
      operationsOk = false;
    }
  }

  if (temporaryExists && rename(temporary, finalPath) == 0) {
    temporaryExists = false;
    finalExists = true;
    result.renameComplete = true;
  } else if (temporaryExists) {
    result.renameError = errno;
    if (result.firstError == 0) result.firstError = errno;
    operationsOk = false;
  }

  if (result.renameComplete) {
    result.directorySyncAfterRename =
        syncDirectory(directory, result.directorySyncAfterRenameError,
                      result.directorySyncCloseFailed);
    if (result.directorySyncAfterRename == LFDS_FAILED) {
      if (result.firstError == 0)
        result.firstError = result.directorySyncAfterRenameError;
      operationsOk = false;
    }

    DIR* dir = opendir(directory);
    if (!dir) {
      result.directoryReadError = errno;
      if (result.firstError == 0) result.firstError = errno;
      operationsOk = false;
    } else {
      bool foundExpected = false;
      bool unexpectedEntry = false;
      errno = 0;
      struct dirent* entry = nullptr;
      while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
          continue;
        ++result.directoryEntries;
        if (strcmp(entry->d_name, "readback.bin") == 0)
          foundExpected = true;
        else
          unexpectedEntry = true;
      }
      const int iterationError = errno;
      const int closeResult = closedir(dir);
      if (iterationError != 0 || closeResult != 0 || !foundExpected ||
          unexpectedEntry || result.directoryEntries != 1) {
        result.directoryReadError = iterationError != 0
            ? iterationError : (closeResult != 0 ? errno : EIO);
        if (result.firstError == 0) result.firstError = result.directoryReadError;
        operationsOk = false;
      } else {
        result.directoryEnumerationComplete = true;
      }
    }

    fd = open(finalPath, O_RDONLY);
    if (fd < 0) {
      result.reopenedReadError = errno;
      if (result.firstError == 0) result.firstError = errno;
      operationsOk = false;
    } else {
      result.reopenedFileOpened = true;
      struct stat reopenedStat;
      uint8_t reopened[kPayloadSize] = {};
      size_t reopenedBytes = 0;
      int readError = 0;
      const int reopenedStatResult = fstat(fd, &reopenedStat);
      result.reopenedReadbackMatches = reopenedStatResult == 0 &&
          S_ISREG(reopenedStat.st_mode) &&
          reopenedStat.st_size == static_cast<off_t>(sizeof expected) &&
          readExact(fd, reopened, sizeof reopened, reopenedBytes, readError) &&
          memcmp(expected, reopened, sizeof expected) == 0;
      if (!result.reopenedReadbackMatches) {
        result.reopenedReadError = reopenedStatResult != 0 ? errno :
            (readError == 0 ? EILSEQ : readError);
        if (result.firstError == 0) result.firstError = result.reopenedReadError;
        operationsOk = false;
      }
      const int closeResult = close(fd);
      result.reopenedCloseComplete = closeResult == 0;
      result.reopenedCloseError = closeResult == 0 ? 0 : errno;
      fd = -1;
      if (!result.reopenedCloseComplete) {
        if (result.firstError == 0) result.firstError = result.reopenedCloseError;
        operationsOk = false;
      }
    }
  }

  int cleanupError = 0;
  bool cleanupOk = true;
  if (fd >= 0) {
    const int closeResult = close(fd);
    if (closeResult != 0) {
      cleanupOk = false;
      cleanupError = errno;
    }
  }
  if (temporaryExists) {
    if (removeOwnedPath(temporary, true, cleanupError))
      temporaryExists = false;
    else
      cleanupOk = false;
  }
  if (finalExists) {
    if (removeOwnedPath(finalPath, true, cleanupError))
      finalExists = false;
    else
      cleanupOk = false;
  }
  if (cleanupOk) {
    result.directorySyncAfterRemove =
        syncDirectory(directory, result.directorySyncAfterRemoveError,
                      result.directorySyncCloseFailed);
    if (result.directorySyncAfterRemove == LFDS_FAILED) {
      if (result.firstError == 0)
        result.firstError = result.directorySyncAfterRemoveError;
      operationsOk = false;
    }
    if (rmdir(directory) == 0) {
      result.directoryRemoved = true;
    } else {
      cleanupError = errno;
      cleanupOk = false;
    }
  }
  result.parentSyncAfterRemove = result.directoryRemoved
      ? syncDirectory(mountpoint, result.parentSyncAfterRemoveError,
                    result.directorySyncCloseFailed)
      : LFDS_NOT_RUN;
  if (result.parentSyncAfterRemove == LFDS_FAILED) {
    if (result.firstError == 0)
      result.firstError = result.parentSyncAfterRemoveError;
    operationsOk = false;
  }

  result.fileRemoved = !temporaryExists && !finalExists;
  result.cleanupComplete = cleanupOk && result.fileRemoved && result.directoryRemoved;
  result.cleanupError = cleanupError;
  if (!result.cleanupComplete) {
    if (result.firstError == 0)
      result.firstError = cleanupError == 0 ? EIO : cleanupError;
    operationsOk = false;
  }

  return operationsOk && result.cleanupComplete && result.writeComplete &&
      result.fileSyncComplete && result.fileStatMatches &&
      result.readbackMatches && result.fileCloseComplete &&
      result.renameComplete && result.directoryEnumerationComplete &&
      result.reopenedReadbackMatches && result.reopenedCloseComplete;
}

}  // namespace att
