// SPDX-License-Identifier: GPL-3.0-or-later
#include "session_display_binding.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QFile>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>
namespace qindaqt::keyring::service {
namespace {
struct Descriptor {
    int value=-1;
    explicit Descriptor(int fd=-1):value(fd) {}
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    ~Descriptor() { if(value>=0) close(value); }
    int release() { const int fd=value;value=-1;return fd; }
};
bool nativeName(const QString &name) {
    const auto prefix=QString(QindaQt::CompositorNames::waylandSocketPrefix);
    if(!name.startsWith(prefix)) return false;
    const auto suffix=name.mid(prefix.size());
    if(suffix.isEmpty() || suffix.size()>4) return false;
    for(const auto c:suffix) if(c<'0' || c>'9') return false;
    bool ok=false;const auto index=suffix.toUInt(&ok);
    return ok && index<=4095 && QString::number(index)==suffix;
}
int privateDirectory(const QString &path) {
    if(!path.startsWith('/') || path.contains("//")) return -1;
    Descriptor current(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    for(const auto &part:path.mid(1).split('/')) {
        if(part.isEmpty() || part=="." || part=="..") return -1;
        Descriptor next(openat(current.value,QFile::encodeName(part).constData(),
            O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        if(next.value<0) return -1;
        close(current.value);current.value=next.release();
    }
    struct stat value{};
    if(fstat(current.value,&value)!=0 || value.st_uid!=geteuid() || (value.st_mode&07777)!=0700) return -1;
    return current.release();
}
}
SessionDisplayBinding::SessionDisplayBinding(QDBusConnection bus,QString runtime,bool required,QObject *parent)
    :QObject(parent),bus_(std::move(bus)),runtime_(std::move(runtime)),required_(required),
      watcher_(QString(QindaQt::CompositorNames::service),bus_,
               QDBusServiceWatcher::WatchForOwnerChange,this) {
    bus_.interface()->setTimeout(250);
    connect(&watcher_,&QDBusServiceWatcher::serviceOwnerChanged,this,
        [this](const QString &,const QString &,const QString &) { if(!basename_.isEmpty()) clear(); });
    check_.setInterval(100);
    connect(&check_,&QTimer::timeout,this,[this] { if(!live()) clear(); });
}
SessionDisplayBinding::~SessionDisplayBinding() { if(probe_>=0) close(probe_);
    if(pidfd_>=0) close(pidfd_); }
int SessionDisplayBinding::connectPeer(const QString &name,qint64 expected,int *pidfd) {
    Descriptor directory(privateDirectory(runtime_));
    if(directory.value<0) return -1;
    struct stat entry{};
    const auto file=QFile::encodeName(name);
    if(fstatat(directory.value,file.constData(),&entry,AT_SYMLINK_NOFOLLOW)!=0
        || !S_ISSOCK(entry.st_mode) || entry.st_uid!=geteuid() || entry.st_nlink!=1 || (entry.st_mode&0007)!=0) return -1;
    Descriptor socketFd(socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));
    if(socketFd.value<0) return -1;
    if(socketFd.value<3) {
        const int duplicate=fcntl(socketFd.value,F_DUPFD_CLOEXEC,3);
        if(duplicate<0) return -1;
        close(socketFd.value);socketFd.value=duplicate;
    }
    sockaddr_un address{};address.sun_family=AF_UNIX;
    const auto endpoint=QByteArray("/proc/self/fd/")+QByteArray::number(directory.value)+'/'+file;
    if(endpoint.size()>=static_cast<qsizetype>(sizeof(address.sun_path))) return -1;
    std::memcpy(address.sun_path,endpoint.constData(),static_cast<std::size_t>(endpoint.size())+1);
    if(::connect(socketFd.value,reinterpret_cast<sockaddr *>(&address),sizeof(address))!=0) return -1;
    ucred peer{};socklen_t size=sizeof(peer);
    if(getsockopt(socketFd.value,SOL_SOCKET,SO_PEERCRED,&peer,&size)!=0
        || peer.uid!=geteuid() || peer.pid!=expected) return -1;
    int held=-1;socklen_t heldSize=sizeof(held);
    if(getsockopt(socketFd.value,SOL_SOCKET,SO_PEERPIDFD,&held,&heldSize)!=0) return -1;
    Descriptor life(held);pollfd status{held,POLLIN,0};
    if(heldSize!=sizeof(held) || poll(&status,1,0)!=0) return -1;
    if(pidfd) *pidfd=life.release();
    return socketFd.release();
}
bool SessionDisplayBinding::attach(const QString &owner,const QString &name) {
    required_=true;clear();
    if(!nativeName(name) || !owner.startsWith(':') || !bus_.isConnected()) return false;
    const auto compositor=bus_.interface()->serviceOwner(QString(QindaQt::CompositorNames::service));
    if(!compositor.isValid() || compositor.value().isEmpty()) return false;
    const auto pid=bus_.interface()->servicePid(compositor.value());
    const auto uid=bus_.interface()->serviceUid(compositor.value());
    const auto session=bus_.interface()->isServiceRegistered(owner);
    if(!pid.isValid() || pid.value()<=1 || !uid.isValid() || uid.value()!=geteuid()
        || !session.isValid() || !session.value()) return false;
    int life=-1;const int socketFd=connectPeer(name,pid.value(),&life);
    if(socketFd<0) return false;
    sessionOwner_=owner;compositorOwner_=compositor.value();compositorPid_=pid.value();
    basename_=name;probe_=socketFd;pidfd_=life;
    watcher_.addWatchedService(owner);check_.start();
    if(!live()) { clear();return false; }
    Q_EMIT attached();
    return true;
}
bool SessionDisplayBinding::sameOwners() {
    if(!bus_.isConnected()) return false;
    const auto current=bus_.interface()->serviceOwner(QString(QindaQt::CompositorNames::service));
    const auto session=bus_.interface()->isServiceRegistered(sessionOwner_);
    return current.isValid() && current.value()==compositorOwner_ && session.isValid() && session.value();
}
bool SessionDisplayBinding::live() {
    if(basename_.isEmpty()) return !required_;
    pollfd life{pidfd_,POLLIN,0},socket{probe_,static_cast<short>(POLLIN|POLLRDHUP),0};
    return poll(&life,1,0)==0 && poll(&socket,1,0)>=0
        && (socket.revents&(POLLHUP|POLLERR|POLLNVAL|POLLRDHUP))==0 && sameOwners();
}
int SessionDisplayBinding::openPromptConnection() {
    if(basename_.isEmpty()) return required_ ? -1 : -2;
    if(!live()) { clear();return -1; }
    const int fd=connectPeer(basename_,compositorPid_);
    if(fd<0 || !sameOwners()) { if(fd>=0) close(fd);clear();return -1; }
    // AGENT-GUARD: hand this exact connected fd to the helper. Reconnecting by
    // basename would permit a same-user pathname replacement after validation.
    return fd;
}
void SessionDisplayBinding::clear() {
    const bool bound=!basename_.isEmpty();
    if(!sessionOwner_.isEmpty()) watcher_.removeWatchedService(sessionOwner_);
    check_.stop();
    if(probe_>=0) close(probe_);
    if(pidfd_>=0) close(pidfd_);
    probe_=-1;pidfd_=-1;compositorPid_=0;
    basename_.clear();sessionOwner_.clear();compositorOwner_.clear();
    if(bound) Q_EMIT revoked();
}
}
