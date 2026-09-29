// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
namespace {
bool run(int argc,char **argv) {
    using namespace qindaqt::keyring::pambridge;
    if (!processProtection() || argc != 4 || std::to_string(geteuid()) != argv[1]) return false;
    Options options;options.runtime=argv[2];
#ifdef QINDAQT_PAM_FIXTURE
    options.fixtureOwner=static_cast<pid_t>(std::stol(argv[3]));
#else
    if (std::string(argv[3]) != "0" || options.runtime != "/run/user/"+std::string(argv[1])+"/qindaqt-keyring") return false;
#endif
    const std::string path=options.runtime+"/control";
    if (path.size() >= sizeof(sockaddr_un::sun_path)) return false;
    Fd socketFd,runtimeFd;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1800);
    while (std::chrono::steady_clock::now() < deadline) {
        Fd candidate(openPrivateRuntime(options.runtime,geteuid()));
        if (candidate.value>=0) {
            struct stat s{};
            if (fstatat(candidate.value,"control",&s,AT_SYMLINK_NOFOLLOW)==0 && S_ISSOCK(s.st_mode) && s.st_uid==geteuid()
                && (s.st_mode & 07777)==0600 && s.st_nlink==1) {
                const int fd=socket(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,0);
                // Pin traversal through the owned directory descriptor. A
                // same-UID path rename cannot retarget this connection.
                const auto endpoint="/proc/self/fd/"+std::to_string(candidate.value)+"/control";
                sockaddr_un address{};address.sun_family=AF_UNIX;
                std::memcpy(address.sun_path,endpoint.c_str(),endpoint.size()+1);
                if (fd >= 0 && connect(fd,reinterpret_cast<sockaddr *>(&address),sizeof(address))==0) {
                    socketFd.value=fd;runtimeFd.value=candidate.value;candidate.value=-1;break;
                }
                if (fd >= 0) close(fd);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (socketFd.value < 0) return false;
    ucred peer{};socklen_t size=sizeof(peer);
    if (getsockopt(socketFd.value,SOL_SOCKET,SO_PEERCRED,&peer,&size)!=0 || peer.uid!=geteuid() || peer.pid<=1) return false;
    // AGENT-GUARD: pidfd_open(peer.pid) can bind a recycled PID rather than
    // this socket peer. SO_PEERPIDFD pins the same kernel credential process.
    int peerFd=-1;socklen_t peerFdSize=sizeof(peerFd);
    if (getsockopt(socketFd.value,SOL_SOCKET,SO_PEERPIDFD,&peerFd,&peerFdSize)!=0
        || peerFdSize!=sizeof(peerFd)) return false;
    Fd pidfd(peerFd);
    if (pidfd.value < 0 || !verifyOwner(geteuid(),peer.pid,pidfd.value,options)) return false;
    pollfd life{pidfd.value,POLLIN,0};
    if (poll(&life,1,0)!=0) return false;
    const unsigned char ready=0;
    if (!exchange(STDOUT_FILENO,std::span(&ready,1),{},200)) return false;
    qindaqt::keyring::SecureBuffer frame(8262);auto bytes=frame.bytes();
    if (!exchange(STDIN_FILENO,{},bytes.first(10),1000)) return false;
    const auto oldSize=(static_cast<std::size_t>(bytes[6])<<8)|bytes[7];
    const auto newSize=(static_cast<std::size_t>(bytes[8])<<8)|bytes[9];
    const std::size_t total=10+bytes[5]+oldSize+newSize;
    if (std::memcmp(bytes.data(),"QKR1",4)!=0 || bytes[5]!=5 || oldSize==0 || oldSize>4096 || newSize>4096
        || (bytes[4]!=1 && bytes[4]!=2) || (bytes[4]==1 && newSize!=0) || (bytes[4]==2 && newSize==0)) return false;
    if (!exchange(STDIN_FILENO,{},bytes.subspan(10,total-10),1000) || std::memcmp(bytes.data()+10,"login",5)!=0) return false;
    // Recheck manager invocation with the same pidfd immediately before token
    // disclosure. Exit/restart cannot retarget this connected Unix descriptor.
    if (poll(&life,1,0)!=0 || !verifyOwner(geteuid(),peer.pid,pidfd.value,options)) return false;
    unsigned char reply[5]{};
    if (!exchange(socketFd.value,bytes.first(total),reply,1400)) return false;
    return std::memcmp(reply,"QKR1",4)==0 && reply[4]==0;
}
}
int main(int argc,char **argv) {
    signal(SIGPIPE,SIG_IGN);
    bool accepted=false;
    try { accepted=run(argc,argv); } catch (...) {}
    const unsigned char result=static_cast<unsigned char>(accepted ? 0 : 1);
    write(STDOUT_FILENO,&result,1);return accepted ? 0 : 1;
}
