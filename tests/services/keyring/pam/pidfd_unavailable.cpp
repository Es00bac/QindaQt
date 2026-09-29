// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/socket.h>
#include <dlfcn.h>
#include <cerrno>
#include <fcntl.h>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
// Direct helper fixture only. Production exec strips LD_* and contains no
// availability override. Simulate an older kernel without SO_PEERPIDFD.
extern "C" int getsockopt(int fd,int level,int option,void *value,socklen_t *length) {
    if (level==SOL_SOCKET && option==SO_PEERPIDFD) { errno=ENOPROTOOPT;return -1; }
    using Original=int (*)(int,int,int,void *,socklen_t *);
    const auto original=reinterpret_cast<Original>(dlsym(RTLD_NEXT,"getsockopt"));
    if (!original) { errno=EINVAL;return -1; }
    return original(fd,level,option,value,length);
}
extern "C" int open(const char *path,int flags,...) {
    mode_t mode=0;
    if ((flags & O_CREAT)!=0 || (flags & O_TMPFILE)==O_TMPFILE) {
        va_list args;va_start(args,flags);mode=va_arg(args,mode_t);va_end(args);
    }
    using Original=int (*)(const char *,int,...);
    const auto original=reinterpret_cast<Original>(dlsym(RTLD_NEXT,"open"));
    if (!original) { errno=EINVAL;return -1; }
    if (std::getenv("QINDAQT_FIXTURE_NO_YAMA")
        && std::strcmp(path,"/proc/sys/kernel/yama/ptrace_scope")==0)
        return original("/dev/zero",O_RDONLY | O_CLOEXEC);
    return original(path,flags,mode);
}
