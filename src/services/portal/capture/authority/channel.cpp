// SPDX-License-Identifier: LGPL-3.0-or-later
#include "channel.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusMessage>
#include <QSocketNotifier>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace QindaQt::Services::Portal::CaptureAuthority {
namespace {
QDBusMessage daemon(const QDBusConnection &bus, const QString &method, const QString &name) {
    auto call = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", method);
    call.setArguments({name}); call.setAutoStartService(false); return bus.call(call, QDBus::Block, 250);
}
bool alive(int fd) { pollfd event{fd, POLLIN, 0}; return fd >= 0 && poll(&event, 1, 0) == 0; }
void closeFds(const std::vector<int> &fds) { for (const int fd : fds) if (fd >= 0) ::close(fd); }
}
ReceivedPacket::~ReceivedPacket() { closeFds(fds); }
Channel::Channel(int fd, Role role, QDBusConnection bus, QObject *parent)
    : QObject(parent), m_fd(fd), m_role(role), m_bus(std::move(bus)) {}
Channel::~Channel() { m_lost = {}; stop(); }
bool Channel::start(std::function<void(ReceivedPacket &&)> receive, std::function<void()> lost) {
    if (m_fd < 0 || m_notifier || m_failed) return false;
    m_receive = std::move(receive); m_lost = std::move(lost);
    ucred peer{}; socklen_t length = sizeof(peer); int type = 0; socklen_t typeLength = sizeof(type); int enabled = 1;
    const int flags = fcntl(m_fd, F_GETFL, 0);
    if (getsockopt(m_fd, SOL_SOCKET, SO_TYPE, &type, &typeLength) || type != SOCK_SEQPACKET
        || getsockopt(m_fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) || length != sizeof(peer) || peer.uid != geteuid() || peer.pid <= 0
        || flags < 0 || fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) || fcntl(m_fd, F_SETFD, FD_CLOEXEC)
        || setsockopt(m_fd, SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled))) { fail(); return false; }
    m_pid = peer.pid; m_pidfd = static_cast<int>(syscall(SYS_pidfd_open, peer.pid, 0));
    const auto owner = daemon(m_bus, "GetNameOwner", QString(QindaQt::CompositorNames::service));
    if (owner.type() == QDBusMessage::ReplyMessage && owner.signature() == "s") m_owner = owner.arguments().value(0).toString();
    if (!identityLive()) { fail(); return false; }
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] { reconcile(); });
    m_handshake.setSingleShot(true);
    connect(&m_handshake, &QTimer::timeout, this, [this] { if (!m_ready) fail(); });
    m_handshake.start(5000); reconcile(); return !m_failed;
}
bool Channel::identityLive() const {
    if (!alive(m_pidfd) || m_owner.isEmpty() || m_fd < 0) return false;
    const auto current = daemon(m_bus, "GetNameOwner", QString(QindaQt::CompositorNames::service));
    if (current.type() != QDBusMessage::ReplyMessage || current.signature() != "s" || current.arguments().value(0).toString() != m_owner) return false;
    const auto pid = daemon(m_bus, "GetConnectionUnixProcessID", m_owner), uid = daemon(m_bus, "GetConnectionUnixUser", m_owner);
    if (pid.type() != QDBusMessage::ReplyMessage || pid.signature() != "u" || pid.arguments().value(0).toLongLong() != m_pid
        || uid.type() != QDBusMessage::ReplyMessage || uid.signature() != "u" || uid.arguments().value(0).toUInt() != geteuid()) return false;
    const auto after = daemon(m_bus, "GetNameOwner", QString(QindaQt::CompositorNames::service));
    return alive(m_pidfd) && after.type() == QDBusMessage::ReplyMessage && after.signature() == "s" && after.arguments().value(0).toString() == m_owner;
}
bool Channel::live() const {
    pollfd event{m_fd, POLLIN | POLLHUP, 0};
    return m_ready && !m_failed && identityLive() && poll(&event, 1, 0) >= 0 && !(event.revents & (POLLHUP | POLLERR | POLLNVAL));
}
bool Channel::transmit(const Packet &packet) {
    const auto bytes = encode(packet); if (bytes.isEmpty() || !identityLive()) { fail(); return false; }
    // SO_PASSCRED on the compositor endpoint supplies post-exec credentials;
    // no user-provided ucred, descriptor, PID or app identity is transmitted.
    const auto count = ::send(m_fd, bytes.constData(), static_cast<size_t>(bytes.size()), MSG_NOSIGNAL | MSG_DONTWAIT);
    if (count != bytes.size()) { fail(); return false; } return true;
}
bool Channel::send(Wire::Message message, quint64 job, const QByteArray &payload) {
    const bool permitted = message == Wire::Message::RevokeJob
        || (m_role == Role::Broker && message == Wire::Message::StartJob)
        || (m_role == Role::Helper && (message == Wire::Message::ParentReady || message == Wire::Message::ConsentGranted));
    if (!permitted || !job || (m_role == Role::Helper && job != m_job) || !live()) { fail(); return false; }
    if (m_role == Role::Broker && message == Wire::Message::StartJob) {
        if (job <= m_highest) { fail(); return false; } m_highest = job;
    }
    if (m_role == Role::Helper) {
        if (message == Wire::Message::RevokeJob) m_revoking = true;
        else if (m_revoking || (message == Wire::Message::ParentReady && m_parent)
            || (message == Wire::Message::ConsentGranted && (!m_parent || m_consent))) { fail(); return false; }
        else if (message == Wire::Message::ParentReady) m_parent = true;
        else if (message == Wire::Message::ConsentGranted) m_consent = true;
    }
    return transmit({message, m_generation, job, payload, 0});
}
void Channel::reconcile() {
    if (m_reading || m_failed || m_fd < 0) return;
    if (!identityLive()) { fail(); return; }
    m_reading = true;
    for (int iteration = 0; iteration < 32 && !m_failed; ++iteration) {
        std::array<char, Wire::HeaderBytes + Wire::MaxPayloadBytes> bytes{};
        alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(ucred)) + CMSG_SPACE(sizeof(int) * Wire::MaxFdsPerMessage)> ancillary{};
        iovec vector{bytes.data(), bytes.size()}; msghdr message{};
        message.msg_iov = &vector; message.msg_iovlen = 1; message.msg_control = ancillary.data(); message.msg_controllen = ancillary.size();
        const auto count = recvmsg(m_fd, &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
        if (count < 0 && errno == EINTR) { --iteration; continue; }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        std::vector<int> fds; std::optional<ucred> credentials; bool valid = count > 0 && !(message.msg_flags & (MSG_TRUNC | MSG_CTRUNC));
        for (auto *item = CMSG_FIRSTHDR(&message); item; item = CMSG_NXTHDR(&message, item)) {
            if (item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_RIGHTS && item->cmsg_len >= CMSG_LEN(0)) {
                const auto size = item->cmsg_len - CMSG_LEN(0); if (!size || size % sizeof(int) || !fds.empty()) valid = false;
                const auto *values = reinterpret_cast<const int *>(CMSG_DATA(item));
                for (size_t i = 0; i < size / sizeof(int); ++i) fds.push_back(values[i]);
            } else if (item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_CREDENTIALS && item->cmsg_len == CMSG_LEN(sizeof(ucred)) && !credentials) {
                ucred value{}; memcpy(&value, CMSG_DATA(item), sizeof(value)); credentials = value;
            } else valid = false;
        }
        const auto packet = count > 0 ? decode(QByteArray(bytes.data(), static_cast<qsizetype>(count))) : std::nullopt;
        if (!valid || !packet || !credentials || credentials->uid != geteuid() || credentials->pid != m_pid
            || fds.size() != packet->descriptors || !identityLive()) { closeFds(fds); fail(); break; }
        if (!m_ready) {
            if (packet->message != Wire::Message::Hello || (m_role == Role::Broker ? packet->job != 0 : packet->job == 0)) { closeFds(fds); fail(); break; }
            m_generation = packet->generation; m_job = packet->job; m_ready = true; m_handshake.stop();
            if (!transmit({Wire::Message::Ready, m_generation, m_job, readyPayload(m_bus.baseService()), 0})) { closeFds(fds); break; }
        } else {
            const bool direction = packet->message == Wire::Message::Error || packet->message == Wire::Message::JobRevoked
                || (m_role == Role::Broker && packet->message == Wire::Message::JobStarted)
                || (m_role == Role::Helper && packet->message == Wire::Message::CaptureReady);
            if (!direction || packet->generation != m_generation || !packet->job || (m_role == Role::Helper && packet->job != m_job)) { closeFds(fds); fail(); break; }
        }
        if (packet->message == Wire::Message::CaptureReady) {
            if (!m_parent || !m_consent || m_granted || m_revoking) { closeFds(fds); fail(); break; } m_granted = true;
        }
        if (m_receive) m_receive(ReceivedPacket(*packet, std::move(fds))); else closeFds(fds);
    }
    m_reading = false;
}
void Channel::fail() { if (m_failed) return; m_failed = true; stop(); if (m_lost) m_lost(); }
void Channel::stop() {
    m_ready = false; m_handshake.stop();
    if (m_notifier) { m_notifier->setEnabled(false); m_notifier->deleteLater(); m_notifier = nullptr; }
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    if (m_pidfd >= 0) { ::close(m_pidfd); m_pidfd = -1; }
}
}
