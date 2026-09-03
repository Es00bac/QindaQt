// SPDX-License-Identifier: GPL-3.0-or-later
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace {
using FstatAt = int (*)(int, const char *, struct stat *, int);

FstatAt resolveRealFstatAt() {
  void *symbol = ::dlsym(RTLD_NEXT, "fstatat");
  FstatAt function = nullptr;
  static_assert(sizeof(function) == sizeof(symbol));
  std::memcpy(&function, &symbol, sizeof(function));
  return function;
}
} // namespace

extern "C" int fstatat(int directory, const char *path, struct stat *status,
                       int flags) {
  static FstatAt realFstatAt = resolveRealFstatAt();
  static bool armed = true;
  const int result = realFstatAt(directory, path, status, flags);
  const int resultError = errno;
  const char *target = std::getenv("QINDAQT_RACE_DESTINATION");
  if (armed && target && path && std::strcmp(path, target) == 0 &&
      result == -1 && resultError == ENOENT) {
    armed = false;
    const int output = ::openat(directory, path,
                                O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                                0600);
    if (output >= 0) {
      constexpr char attacker[] = "attacker-content";
      const ssize_t ignored = ::write(output, attacker, sizeof(attacker) - 1);
      (void)ignored;
      ::close(output);
    }
  }
  errno = resultError;
  return result;
}
