// SPDX-License-Identifier: GPL-3.0-or-later
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
namespace {
bool fail(const char *call, int descriptor) {
  const char *selected = ::getenv("QINDAQT_ED05_FAIL_CALL");
  if (!selected || std::strcmp(selected, call) != 0) return false;
  char descriptorName[64] {}, path[4096] {};
  const int size = ::snprintf(descriptorName, sizeof(descriptorName), "/proc/self/fd/%d", descriptor);
  if (size <= 0 || static_cast<size_t>(size) >= sizeof(descriptorName)) return false;
  const ssize_t length = ::readlink(descriptorName, path, sizeof(path) - 1);
  if (length < 0) return false;
  path[static_cast<size_t>(length)] = '\0';
  if (!std::strstr(path, "/.qindaqt-stage-") || !std::strstr(path, "/payload")) return false;
  errno = std::strcmp(call, "fchmod") == 0 || std::strcmp(call, "futimens") == 0 ? EPERM : ENOSPC;
  return true;
}
}
extern "C" int fchmod(int descriptor, mode_t mode) {
  if (fail("fchmod", descriptor)) return -1;
  const auto real = reinterpret_cast<int (*)(int, mode_t)>(::dlsym(RTLD_NEXT, "fchmod"));
  return real ? real(descriptor, mode) : -1;
}
extern "C" int futimens(int descriptor, const struct timespec times[2]) {
  if (fail("futimens", descriptor)) return -1;
  const auto real = reinterpret_cast<int (*)(int, const struct timespec *)>(::dlsym(RTLD_NEXT, "futimens"));
  return real ? real(descriptor, times) : -1;
}
extern "C" int fsync(int descriptor) {
  if (fail("fsync", descriptor)) return -1;
  const auto real = reinterpret_cast<int (*)(int)>(::dlsym(RTLD_NEXT, "fsync"));
  return real ? real(descriptor) : -1;
}
extern "C" ssize_t write(int descriptor, const void *buffer, size_t size) {
  if (fail("write", descriptor)) return -1;
  const auto real = reinterpret_cast<ssize_t (*)(int, const void *, size_t)>(::dlsym(RTLD_NEXT, "write"));
  return real ? real(descriptor, buffer, size) : -1;
}

extern "C" int qindaqt_ed05_fault_loaded() { return 1; }
