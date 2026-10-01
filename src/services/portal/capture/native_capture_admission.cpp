// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_capture_admission.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusMessage>
#include <poll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
namespace {
QDBusMessage daemon(QDBusConnection bus, const QString &method, const QString &name) {
    auto call = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", method);
    call.setArguments({name}); call.setAutoStartService(false); return bus.call(call, QDBus::Block, 250);
}
}
NativeCaptureAdmission::NativeCaptureAdmission(QDBusConnection bus, QString owner, int fd)
    : m_bus(std::move(bus)), m_owner(std::move(owner)), m_transport(m_bus),
      m_monitor(m_transport, [this](const QString &name, quint64 pid) { return identityLive(name, pid); }) {
    ucred peer{}; socklen_t length = sizeof(peer);
    if (fd >= 0 && getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) == 0 && length == sizeof(peer) && peer.uid == geteuid() && peer.pid > 0) {
        m_pid = static_cast<quint64>(peer.pid); m_pidfd = static_cast<int>(syscall(SYS_pidfd_open, peer.pid, 0));
    }
    connect(&m_monitor, &SessionLockState::NativeLockStateMonitor::contentMayBeShownChanged, this, [this](bool shown) {
        if (shown && admitted()) { m_once = true; Q_EMIT ready(); }
        else if (m_once) Q_EMIT lost();
    });
    m_lifetime.setInterval(50);
    connect(&m_lifetime, &QTimer::timeout, this, [this] { if (m_once && !admitted()) Q_EMIT lost(); });
    m_lifetime.start(); m_monitor.start();
}
NativeCaptureAdmission::~NativeCaptureAdmission() { m_monitor.stop(); if (m_pidfd >= 0) ::close(m_pidfd); }
bool NativeCaptureAdmission::identityLive(const QString &owner, quint64 pid) const {
    pollfd event{m_pidfd, POLLIN, 0};
    if (m_pidfd < 0 || poll(&event, 1, 0) != 0 || owner != m_owner || pid != m_pid || !m_pid) return false;
    const auto current = daemon(m_bus, "GetNameOwner", QString(QindaQt::CompositorNames::service));
    if (current.type() != QDBusMessage::ReplyMessage || current.signature() != "s" || current.arguments().value(0).toString() != m_owner) return false;
    const auto actual = daemon(m_bus, "GetConnectionUnixProcessID", m_owner);
    return actual.type() == QDBusMessage::ReplyMessage && actual.signature() == "u" && actual.arguments().value(0).toULongLong() == m_pid;
}
bool NativeCaptureAdmission::admitted() const { return identityLive(m_owner, m_pid) && m_monitor.contentMayBeShown(); }
bool NativeCaptureAdmission::lineageLive() const { return identityLive(m_owner, m_pid); }
}
