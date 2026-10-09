// SPDX-License-Identifier: GPL-3.0-or-later
#include "packet_channel_p.h"
#include "qindaqt/services/android_runtime/descriptors.h"
#include <QFile>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace QindaQt::Services::AndroidRuntime {
namespace {
bool endpointConnected(int fd)
{
    pollfd endpoint{fd,POLLRDHUP,0};
    return ::poll(&endpoint,1,0)>=0
        && (endpoint.revents&(POLLRDHUP|POLLHUP|POLLERR|POLLNVAL))==0;
}
bool pidfdMatches(int fd,int pid)
{
    std::array<char,64> target{};
    const auto path=QStringLiteral("/proc/self/fd/%1").arg(fd).toLocal8Bit();
    const auto size=::readlink(path.constData(),target.data(),target.size());
    if (size!=18 || QByteArray(target.data(),size)!=QByteArrayLiteral("anon_inode:[pidfd]")) return false;
    QFile info(QStringLiteral("/proc/self/fdinfo/%1").arg(fd));
    if (!info.open(QIODevice::ReadOnly)) return false;
    const auto bytes=info.read(4097); if (bytes.size()>4096) return false;
    for (const auto &line:bytes.split('\n')) {
        if (line.startsWith("Pid:")) {
            bool ok=false; const int actual=line.mid(4).trimmed().toInt(&ok);
            return ok && actual==pid;
        }
    }
    return false;
}
bool configure(int fd) {
    const int enabled=1;
    const auto flags=::fcntl(fd,F_GETFL); const auto descriptorFlags=::fcntl(fd,F_GETFD);
    return flags>=0 && descriptorFlags>=0
        && ::fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0
        && ::fcntl(fd,F_SETFD,descriptorFlags|FD_CLOEXEC)==0
        && ::setsockopt(fd,SOL_SOCKET,SO_PASSCRED,&enabled,sizeof(enabled))==0;
}
}
std::array<int,2> createPrivateProducerDescriptors()
{
    std::array<int,2> pair{-1,-1};
    if (::socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0,pair.data())!=0) return {-1,-1};
    if (!configure(pair[0]) || !configure(pair[1])) { ::close(pair[0]); ::close(pair[1]); return {-1,-1}; }
    return pair;
}
int holdProducerLifetime(int observedPid)
{
    if (observedPid<=0) return -1;
    const long result=::syscall(SYS_pidfd_open,observedPid,0);
    return result<0 ? -1 : static_cast<int>(result);
}
PacketChannel::PacketChannel(int socketFd,int lifetimeFd,int executableFd,int expectedPid,unsigned expectedUid)
    : m_socket(socketFd),m_lifetime(lifetimeFd),m_executable(executableFd),m_pid(expectedPid),m_uid(expectedUid)
{
    if (m_socket==m_lifetime || m_socket==m_executable || m_lifetime==m_executable) {
        if (m_socket==m_lifetime) m_lifetime=-1;
        if (m_socket==m_executable || m_lifetime==m_executable) m_executable=-1;
        close(); return;
    }
    int type=0; socklen_t size=sizeof(type);
    m_valid=m_socket>=0 && m_lifetime>=0 && m_pid>0
        && ::getsockopt(m_socket,SOL_SOCKET,SO_TYPE,&type,&size)==0 && type==SOCK_SEQPACKET
        && configure(m_socket) && pidfdMatches(m_lifetime,m_pid)
        && m_executable>=0 && ::fstat(m_executable,&m_pin)==0 && S_ISREG(m_pin.st_mode);
    if (!m_valid) close();
}
PacketChannel::~PacketChannel() { close(); }
void PacketChannel::close()
{
    m_valid=false;
    if (m_socket>=0) ::close(m_socket);
    if (m_lifetime>=0) ::close(m_lifetime);
    if (m_executable>=0) ::close(m_executable);
    m_socket=m_lifetime=m_executable=-1;
}
bool PacketChannel::live() const
{
    // AGENT-GUARD: a still-live renderer can close its private endpoint.
    // Cached origin must withdraw before the queued socket notifier too.
    if (!m_valid || !endpointConnected(m_socket)) return false;
    pollfd lifetime{m_lifetime,POLLIN,0};
    // Every getter/send/read rechecks the held lifetime, before queued Qt
    // death notification. A reused numeric PID never replaces this pidfd.
    if (::poll(&lifetime,1,0)!=0) return false;
    const auto path=QStringLiteral("/proc/%1/exe").arg(m_pid).toLocal8Bit();
    const int executable=::open(path.constData(),O_PATH|O_CLOEXEC);
    if (executable<0) return false;
    struct stat current{}; const bool observed=::fstat(executable,&current)==0;
    ::close(executable);
    return observed && current.st_dev==m_pin.st_dev && current.st_ino==m_pin.st_ino
        && current.st_size==m_pin.st_size && current.st_mode==m_pin.st_mode
        && current.st_mtim.tv_sec==m_pin.st_mtim.tv_sec && current.st_mtim.tv_nsec==m_pin.st_mtim.tv_nsec
        && current.st_ctim.tv_sec==m_pin.st_ctim.tv_sec && current.st_ctim.tv_nsec==m_pin.st_ctim.tv_nsec
        && ::poll(&lifetime,1,0)==0 && endpointConnected(m_socket);
}
ReadPacket PacketChannel::read()
{
    if (!live()) return {ReadPacket::Lost,{}};
    QByteArray bytes(maxPacketBytes,'\0');
    alignas(cmsghdr) std::array<char,CMSG_SPACE(sizeof(ucred))+CMSG_SPACE(16*sizeof(int))> control{};
    iovec buffer{bytes.data(),static_cast<size_t>(bytes.size())};
    msghdr message{}; message.msg_iov=&buffer; message.msg_iovlen=1;
    message.msg_control=control.data(); message.msg_controllen=control.size();
    const auto size=::recvmsg(m_socket,&message,MSG_DONTWAIT|MSG_CMSG_CLOEXEC);
    if (size<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return {};
    if (size<=0) return {ReadPacket::Lost,{}};
    bool found=false; bool rejected=(message.msg_flags&(MSG_TRUNC|MSG_CTRUNC))!=0;
    for (auto *item=CMSG_FIRSTHDR(&message);item;item=CMSG_NXTHDR(&message,item)) {
        if (item->cmsg_level==SOL_SOCKET && item->cmsg_type==SCM_RIGHTS) {
            const size_t count=(item->cmsg_len-CMSG_LEN(0))/sizeof(int);
            const auto *fds=reinterpret_cast<const int *>(CMSG_DATA(item));
            for (size_t i=0;i<count;++i) ::close(fds[i]);
            rejected=true;
        } else if (item->cmsg_level==SOL_SOCKET && item->cmsg_type==SCM_CREDENTIALS
                   && item->cmsg_len==CMSG_LEN(sizeof(ucred)) && !found) {
            ucred sender{}; std::memcpy(&sender,CMSG_DATA(item),sizeof(sender));
            found=true; rejected |= sender.pid!=m_pid || sender.uid!=m_uid;
        } else { rejected=true; }
    }
    // AGENT-GUARD: SO_PEERCRED on a pre-fork socketpair names its creator.
    // Only kernel SCM_CREDENTIALS for this packet identifies the child sender.
    if (!found || rejected || !live()) return {ReadPacket::Rejected,{}};
    bytes.resize(size); return {ReadPacket::Received,bytes};
}
bool PacketChannel::write(const QByteArray &bytes)
{
    if (!live() || bytes.isEmpty() || bytes.size()>maxPacketBytes) return false;
    const auto size=::send(m_socket,bytes.constData(),static_cast<size_t>(bytes.size()),MSG_DONTWAIT|MSG_NOSIGNAL);
    return size==bytes.size(); // seqpacket is atomic; never queue/replay a mutation
}
} // namespace QindaQt::Services::AndroidRuntime
