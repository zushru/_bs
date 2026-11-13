#pragma once

// Host-only syscall overrides for deterministic LittleFsFileSet I/O tests.
// This is not an ESP32 runtime abstraction and must not be used by product code.
#if defined(ESP_PLATFORM)
#error "LittleFsFileSet host test hooks are unavailable on ESP32"
#endif

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

namespace att {

struct LittleFsFileSetTestOps {
  void* context;
  ssize_t (*writeFn)(void* context, int fd, const void* data, size_t count);
  int (*fsyncFn)(void* context, int fd);
  ssize_t (*preadFn)(void* context, int fd, void* data, size_t count,
                     off_t offset);
  int (*closeFn)(void* context, int fd);
  int (*statFn)(void* context, const char* path, struct stat* st);
  int (*openFn)(void* context, const char* path, int flags, mode_t mode);
  int (*fstatFn)(void* context, int fd, struct stat* st);
  DIR* (*opendirFn)(void* context, const char* path);
  struct dirent* (*readdirFn)(void* context, DIR* dir);
  int (*closedirFn)(void* context, DIR* dir);
};

}  // namespace att
