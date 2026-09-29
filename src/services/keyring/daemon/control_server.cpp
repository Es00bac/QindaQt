// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_server.h"
#include <QTimer>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

namespace qindaqt::keyring::service {
namespace {
[[noreturn]] void fail() { throw std::runtime_error("Control unavailable"); }
unsigned int length(const std::span<const unsigned char> bytes, std::size_t offset) {
    return (static_cast<unsigned int>(bytes[offset]) << 8) | bytes[offset+1];
}
}
ControlServer::Client::~Client() { if (fd >= 0) close(fd); }
ControlServer::ControlServer(QString directory,CollectionRepository &repository,int activatedFd,QObject *parent)
    : QObject(parent),path_(directory + "/control"),runtime_(directory),repository_(repository) {
    const auto path = path_.toUtf8();
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    if (path.size() >= static_cast<qsizetype>(sizeof(address.sun_path))) fail();
    std::memcpy(address.sun_path,path.constData(),static_cast<std::size_t>(path.size())+1);
    if (activatedFd >= 0) {
        sockaddr_un actual{}; socklen_t size = sizeof(actual);
        int accepting = 0; socklen_t optionSize = sizeof(accepting);
        struct stat s{};
        if (getsockname(activatedFd,reinterpret_cast<sockaddr *>(&actual),&size) != 0 || actual.sun_family != AF_UNIX
            || strnlen(actual.sun_path,sizeof(actual.sun_path)) != static_cast<std::size_t>(path.size())
            || std::memcmp(actual.sun_path,path.constData(),static_cast<std::size_t>(path.size())) != 0
            || getsockopt(activatedFd,SOL_SOCKET,SO_ACCEPTCONN,&accepting,&optionSize) != 0 || accepting != 1
            || fstatat(runtime_.descriptor(),"control",&s,AT_SYMLINK_NOFOLLOW) != 0 || !S_ISSOCK(s.st_mode)
            || s.st_uid != geteuid() || (s.st_mode & 07777) != 0600) fail();
        listener_ = activatedFd;
        if (fcntl(listener_,F_SETFD,FD_CLOEXEC) != 0 || fcntl(listener_,F_SETFL,O_NONBLOCK) != 0) fail();
    } else {
        int fd = socket(AF_UNIX,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
        if (fd < 0) fail();
        struct stat s{};
        if (fstatat(runtime_.descriptor(),"control",&s,AT_SYMLINK_NOFOLLOW) == 0) {
            if (!S_ISSOCK(s.st_mode) || s.st_uid != geteuid() || s.st_nlink != 1 || (s.st_mode & 07777) != 0600
                || ::connect(fd,reinterpret_cast<sockaddr *>(&address),sizeof(address)) == 0 || errno != ECONNREFUSED) {
                close(fd); fail();
            }
            if (unlinkat(runtime_.descriptor(),"control",0) != 0) { close(fd); fail(); }
        } else if (errno != ENOENT) { close(fd); fail(); }
        // Bind relative to the already-pinned private directory, never a
        // raceable chain of path components. getsockname may show proc-fd path.
        const auto bound = QByteArray("/proc/self/fd/") + QByteArray::number(runtime_.descriptor()) + "/control";
        sockaddr_un pinned{}; pinned.sun_family = AF_UNIX;
        if (bound.size() >= static_cast<qsizetype>(sizeof(pinned.sun_path))) { close(fd); fail(); }
        std::memcpy(pinned.sun_path,bound.constData(),static_cast<std::size_t>(bound.size())+1);
        if (bind(fd,reinterpret_cast<sockaddr *>(&pinned),sizeof(pinned)) != 0
            || fchmodat(runtime_.descriptor(),"control",0600,0) != 0 || listen(fd,8) != 0 || fsync(runtime_.descriptor()) != 0) {
            close(fd); unlinkat(runtime_.descriptor(),"control",0); fail();
        }
        listener_ = fd; ownsPath_ = true;
    }
    notifier_ = std::make_unique<QSocketNotifier>(listener_,QSocketNotifier::Read,this);
    connect(notifier_.get(),&QSocketNotifier::activated,this,[this] { accept(); });
}
ControlServer::~ControlServer() {
    clients_.clear(); notifier_.reset();
    if (listener_ >= 0) close(listener_);
    if (ownsPath_) { unlinkat(runtime_.descriptor(),"control",0); fsync(runtime_.descriptor()); }
}
void ControlServer::accept() {
    for (;;) {
        const int fd = accept4(listener_,nullptr,nullptr,SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) return;
        ucred peer{}; socklen_t length = sizeof(peer);
        if (getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&length) != 0 || peer.uid != geteuid() || clients_.size() >= 8) {
            close(fd); continue;
        }
        const auto id = ++generation_;
        auto client = std::make_unique<Client>(); client->fd = fd;
        client->notifier = std::make_unique<QSocketNotifier>(fd,QSocketNotifier::Read,this);
        connect(client->notifier.get(),&QSocketNotifier::activated,this,[this,id] { receive(id); });
        clients_.emplace(id,std::move(client));
        QTimer::singleShot(2000,this,[this,id] { finish(id,false); });
    }
}
void ControlServer::receive(quint64 id) {
    const auto found = clients_.find(id); if (found == clients_.end()) return;
    auto &c = *found->second;
    const auto n = recv(c.fd,c.frame.bytes().data()+c.used,c.frame.size()-c.used,0);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) return;
    if (n <= 0) { finish(id,false); return; }
    c.used += static_cast<std::size_t>(n);
    if (c.used < 10) return;
    const auto bytes = c.frame.bytes();
    const auto oldSize = length(bytes,6), newSize = length(bytes,8);
    const auto idSize = bytes[5];
    const auto total = std::size_t(10) + idSize + oldSize + newSize;
    if (std::memcmp(bytes.data(),"QKR1",4) != 0 || idSize == 0 || idSize > 60 || oldSize > 4096 || newSize > 4096
        || bytes[4] < 1 || bytes[4] > 3 || c.used > total
        || (bytes[4] == 1 && (oldSize == 0 || newSize != 0))
        || (bytes[4] == 2 && (oldSize == 0 || newSize == 0))
        || (bytes[4] == 3 && (oldSize != 0 || newSize != 0))) { finish(id,false); return; }
    if (c.used < total) return;
    c.notifier->setEnabled(false);
    const QString collection = QString::fromUtf8(reinterpret_cast<const char *>(bytes.data()+10),idSize);
    if (!validName(collection) || collection == "session") { finish(id,false); return; }
    const auto password = bytes.subspan(10+idSize,oldSize);
    try {
        if (bytes[4] == 3) { repository_.lock(collection); Q_EMIT collectionStateChanged(collection); finish(id,true); }
        else if (!repository_.unlock(collection,password)) { Q_EMIT collectionStateChanged(collection); finish(id,false); }
        else if (bytes[4] == 1) { Q_EMIT collectionStateChanged(collection); finish(id,true); }
        else {
            const bool wasLocked = true; // Authentication may publish unlocked state; cancellation retires it.
            QTimer::singleShot(550,this,[this,id,collection,idSize,oldSize,newSize,wasLocked] {
                const auto pending = clients_.find(id); if (pending == clients_.end()) return;
                try {
                    unsigned char probe = 0;
                    const auto connected = recv(pending->second->fd,&probe,1,MSG_PEEK | MSG_DONTWAIT);
                    if (connected == 0 || connected > 0 || (connected < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        if (wasLocked) repository_.lock(collection);
                        Q_EMIT collectionStateChanged(collection); finish(id,false); return;
                    }
                    const auto newPassword = pending->second->frame.bytes().subspan(10+idSize+oldSize,newSize);
                    const bool success = repository_.rekey(collection,newPassword);
                    Q_EMIT collectionStateChanged(collection); finish(id,success);
                } catch (const std::exception &) { finish(id,false); }
            });
        }
    } catch (const std::exception &) { finish(id,false); }
}
void ControlServer::finish(quint64 id,bool success) {
    const auto found = clients_.find(id); if (found == clients_.end()) return;
    auto c = std::move(found->second); clients_.erase(found);
    const unsigned char response[] = {'Q','K','R','1',static_cast<unsigned char>(success ? 0 : 1)};
    send(c->fd,response,sizeof(response),MSG_NOSIGNAL | MSG_DONTWAIT);
    c->notifier->setEnabled(false);
    c->notifier.release()->deleteLater(); // Never delete signal sender on its stack.
}
}
