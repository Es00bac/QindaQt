// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <poll.h>
#include <unistd.h>
#include <chrono>
#include <cerrno>
#include <cstring>
namespace qindaqt::keyring::pambridge {
Fd::~Fd() { if (value >= 0) close(value); }
namespace {
bool inspect(const std::string &path,uid_t owner,bool directory,bool executable) {
    const int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC | (directory ? O_DIRECTORY : O_NONBLOCK);
    Fd fd(open(path.c_str(),flags)); struct stat s{};
    return fd.value >= 0 && fstat(fd.value,&s) == 0 && s.st_uid == owner
        && (s.st_mode & 0022) == 0 && (directory ? S_ISDIR(s.st_mode) : S_ISREG(s.st_mode))
        && (!executable || (s.st_mode & 0111) != 0);
}
}
bool protectedFile(const std::string &path,uid_t owner,bool executable) {
    if (path.empty() || path[0] != '/' || path.find("//") != std::string::npos) return false;
    for (std::size_t end = 1; (end = path.find('/',end)) != std::string::npos; ++end) {
        const auto prefix = path.substr(0,end);
        if (!inspect(prefix,0,true,false) && (owner == 0 || !inspect(prefix,owner,true,false))) return false;
    }
    return inspect(path,owner,false,executable);
}
int openPrivateRuntime(const std::string &path,uid_t owner) {
    if (path.empty() || path[0] != '/' || path.find("//") != std::string::npos) return -1;
    Fd current(open("/",O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    for (std::size_t begin = 1; begin < path.size();) {
        const auto end = path.find('/',begin);
        const auto part = path.substr(begin,end == std::string::npos ? end : end-begin);
        if (part.empty() || part == "." || part == "..") return -1;
        const int next = openat(current.value,part.c_str(),O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0) return -1;
        close(current.value); current.value = next;
        if (end == std::string::npos) break;
        begin = end+1;
    }
    struct stat s{};
    if (fstat(current.value,&s)!=0 || s.st_uid!=owner || (s.st_mode & 07777)!=0700) return -1;
    const int owned=current.value;current.value=-1;return owned;
}
bool privateRuntime(const std::string &path,uid_t owner) {
    Fd directory(openPrivateRuntime(path,owner));return directory.value>=0;
}
bool ptracePolicySupported() {
    Fd policy(open("/proc/sys/kernel/yama/ptrace_scope",O_RDONLY | O_CLOEXEC));
    char value = 0;
    return policy.value >= 0 && read(policy.value,&value,1)==1 && value>='1' && value<='3';
}
bool processProtection() {
    struct rlimit core{0,0};
    return ptracePolicySupported() && setrlimit(RLIMIT_CORE,&core)==0 && prctl(PR_SET_DUMPABLE,0)==0;
}
bool exchange(int fd,std::span<const unsigned char> outgoing,std::span<unsigned char> incoming,int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    std::size_t sent = 0,received = 0;
    while (sent < outgoing.size() || received < incoming.size()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if (left <= 0) return false;
        pollfd wait{fd,static_cast<short>(sent < outgoing.size() ? POLLOUT : POLLIN),0};
        const int result = poll(&wait,1,static_cast<int>(left));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0 || (wait.revents & (POLLERR | POLLNVAL)) != 0) return false;
        const auto count = sent < outgoing.size() ? send(fd,outgoing.data()+sent,outgoing.size()-sent,MSG_NOSIGNAL)
            : read(fd,incoming.data()+received,incoming.size()-received);
        if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (count <= 0) return false;
        if (sent < outgoing.size()) sent += static_cast<std::size_t>(count);
        else received += static_cast<std::size_t>(count);
    }
    return true;
}
}
