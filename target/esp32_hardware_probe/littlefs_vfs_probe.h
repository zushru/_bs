#pragma once

#include <stddef.h>
#include <stdint.h>

namespace att {

enum LittleFsDirectorySyncResult {
  LFDS_NOT_RUN = 0,
  LFDS_SUPPORTED,
  LFDS_UNSUPPORTED,
  LFDS_FAILED
};

struct LittleFsVfsProbeResult {
  bool directoryCreated;
  bool fileCreated;
  bool writeComplete;
  bool fileSyncComplete;
  bool fileStatMatches;
  bool readbackMatches;
  bool fileCloseComplete;
  bool renameComplete;
  bool directoryEnumerationComplete;
  bool reopenedFileOpened;
  bool reopenedReadbackMatches;
  bool reopenedCloseComplete;
  bool directorySyncCloseFailed;
  bool fileRemoved;
  bool directoryRemoved;
  bool cleanupComplete;
  size_t bytesWritten;
  size_t bytesRead;
  size_t directoryEntries;
  int firstError;
  int writeError;
  int fileSyncError;
  int fileStatError;
  int readError;
  int fileCloseError;
  int renameError;
  int directoryReadError;
  int reopenedReadError;
  int reopenedCloseError;
  int cleanupError;
  LittleFsDirectorySyncResult parentSyncAfterCreate;
  LittleFsDirectorySyncResult directorySyncAfterRename;
  LittleFsDirectorySyncResult directorySyncAfterRemove;
  LittleFsDirectorySyncResult parentSyncAfterRemove;
  int parentSyncAfterCreateError;
  int directorySyncAfterRenameError;
  int directorySyncAfterRemoveError;
  int parentSyncAfterRemoveError;
};

bool runLittleFsVfsProbe(const char* mountpoint, uint32_t runId,
                         LittleFsVfsProbeResult& result);

}  // namespace att
