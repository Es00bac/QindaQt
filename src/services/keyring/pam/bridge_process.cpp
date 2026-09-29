// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <thread>
namespace qindaqt::keyring::pambridge {
bool bridge(Identity identity,const Options &options,unsigned char operation,
            std::span<const unsigned char> oldToken,std::span<const unsigned char> newToken) {
    if (oldToken.empty() || oldToken.size() > 4096 || newToken.size() > 4096
        || (operation != 1 && operation != 2) || (operation == 2 && newToken.empty())
        || (operation == 1 && !newToken.empty())) return false;
    const std::string program = QINDAQT_PAM_HELPER;
#ifdef QINDAQT_PAM_FIXTURE
    const uid_t fileOwner = geteuid();
#else
    constexpr uid_t fileOwner = 0;
#endif
    if (!ptracePolicySupported() || !protectedFile(program,fileOwner,true) || (geteuid() != 0 && geteuid() != identity.uid)) return false;
    if (!startOwner(identity.uid)) return false;
    // Allocate before fork so failure cannot abandon a helper.
    SecureBuffer frame(15+oldToken.size()+newToken.size());
    int pipes[2]{};
    if (socketpair(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC,0,pipes) != 0) return false;
    Fd originalParent(pipes[0]),originalChild(pipes[1]);
    Fd parent(fcntl(originalParent.value,F_DUPFD_CLOEXEC,3));
    Fd child(fcntl(originalChild.value,F_DUPFD_CLOEXEC,3));
    if (parent.value < 0 || child.value < 0) return false;
    close(originalParent.value);originalParent.value=-1;
    close(originalChild.value);originalChild.value=-1;
    const int mode = fcntl(parent.value,F_GETFL);
    if (mode < 0 || fcntl(parent.value,F_SETFL,mode | O_NONBLOCK) < 0) return false;
    const auto uid = std::to_string(identity.uid);
    const auto runtime = options.runtime.empty() ? "/run/user/"+uid+"/qindaqt-keyring" : options.runtime;
    const auto fixture = std::to_string(
#ifdef QINDAQT_PAM_FIXTURE
        options.fixtureOwner
#else
        0
#endif
    );
    const std::array<char *,8> args{const_cast<char *>(program.c_str()),const_cast<char *>(uid.c_str()),
        const_cast<char *>(runtime.c_str()),const_cast<char *>(fixture.c_str()),nullptr,nullptr,nullptr,nullptr};
    char path[] = "PATH=/usr/bin:/bin"; char locale[] = "LANG=C.UTF-8";
    char *environment[]{path,locale,nullptr};
    const rlimit core{0,0};
    const pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // PAM may own independent ordinary heap copies. Suppress child dumps
        // immediately and again after credential changes, until exec retires
        // that address space. The parent's policy check fences the exec gap.
        if (syscall(SYS_prlimit64,0,RLIMIT_CORE,&core,nullptr)!=0
            || syscall(SYS_prctl,PR_SET_DUMPABLE,0,0,0,0)!=0) _exit(2);
        // AGENT-GUARD: SecureBuffer mappings are DONTFORK. The child accesses
        // only prebuilt non-secret argv until exec; token delivery follows its
        // trusted-peer readiness acknowledgement (ADR-0300).
        if (dup2(child.value,STDIN_FILENO) < 0 || dup2(child.value,STDOUT_FILENO) < 0) _exit(2);
        close(parent.value);
        if (geteuid() == 0 && (syscall(SYS_setgroups,0,nullptr) != 0
            || syscall(SYS_setresgid,identity.gid,identity.gid,identity.gid) != 0
            || syscall(SYS_setresuid,identity.uid,identity.uid,identity.uid) != 0)) _exit(2);
        if (syscall(SYS_prctl,PR_SET_DUMPABLE,0,0,0,0)!=0) _exit(2);
        if (syscall(SYS_close_range,3U,~0U,0U)!=0) _exit(2);
        execve(program.c_str(),args.data(),environment);
        _exit(2);
    }
    close(child.value); child.value = -1;
    unsigned char ready = 1;
    bool success = exchange(parent.value,{},std::span(&ready,1),2200) && ready == 0;
    if (success) {
        auto bytes = frame.bytes();
        std::memcpy(bytes.data(),"QKR1",4); bytes[4]=operation; bytes[5]=5;
        bytes[6]=static_cast<unsigned char>(oldToken.size() >> 8);bytes[7]=static_cast<unsigned char>(oldToken.size());
        bytes[8]=static_cast<unsigned char>(newToken.size() >> 8);bytes[9]=static_cast<unsigned char>(newToken.size());
        std::memcpy(bytes.data()+10,"login",5);
        std::copy(oldToken.begin(),oldToken.end(),bytes.begin()+15);
        std::copy(newToken.begin(),newToken.end(),bytes.begin()+15+static_cast<std::ptrdiff_t>(oldToken.size()));
        unsigned char result = 1;
        success = exchange(parent.value,bytes,std::span(&result,1),1800) && result == 0;
    }
    shutdown(parent.value,SHUT_RDWR);
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
    pid_t reaped;
    do {
        reaped=waitpid(pid,&status,WNOHANG);
        if (reaped==0) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (reaped==0 && std::chrono::steady_clock::now() < deadline);
    if (reaped==0) { kill(pid,SIGKILL); while (waitpid(pid,&status,0)<0 && errno==EINTR) {} }
    return success;
}
}
