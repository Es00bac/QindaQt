// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_input_gate.h"

#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

namespace QindaQt::Link {
namespace {
using Platform::Compositor::CompositorAttachment;
using Platform::Compositor::PeerExpectation;
using Services::SessionLockState::NativeLockStateMonitor;
using Services::SessionLockState::QtNativeLockTransport;
constexpr auto kSession1 = "org.qindaqt.Session1";

// Kernel-reported PID of whoever listens on the Wayland socket; 0 on failure.
quint64 peerPid(const QString &path)
{
    const QByteArray bytes = path.toLocal8Bit();
    sockaddr_un addr{};
    if (bytes.size() >= int(sizeof(addr.sun_path))) return 0;
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, bytes.constData(), size_t(bytes.size()));
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return 0;
    ucred cred{};
    socklen_t len = sizeof(cred);
    const bool ok = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0
        && ::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0;
    ::close(fd);
    return ok && cred.pid > 1 ? quint64(cred.pid) : 0;
}
}

NativeInputGate::NativeInputGate(QDBusConnection bus, QString runtimeDirectory, QString socketBasename, QObject *parent)
    : InputGate(parent), m_bus(std::move(bus)), m_runtime(std::move(runtimeDirectory)), m_socket(std::move(socketBasename)),
      m_watcher(this)
{
    m_watcher.setConnection(m_bus);
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    m_watcher.setWatchedServices({QString::fromLatin1(kSession1), QString(CompositorNames::service)});
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &NativeInputGate::rebind);
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] { m_attempts = 0; rebind(); });
    // The session supervisor publishes Session1 after resident services start.
    QTimer::singleShot(0, this, &NativeInputGate::rebind);
}

NativeInputGate::~NativeInputGate() { teardown(); }

bool NativeInputGate::open() const { return m_monitor && m_attachment && m_attachment->live() && m_monitor->contentMayBeShown(); }

void NativeInputGate::teardown()
{
    if (m_monitor) m_monitor->stop();
    m_monitor.reset();
    m_transport.reset();
    if (m_attachment) m_attachment->revoke();
    m_attachment.reset();
}

void NativeInputGate::rebind()
{
    const bool wasOpen = open();
    teardown();
    auto finish = [this, wasOpen] { if (wasOpen) Q_EMIT changed(); };
    if (!m_bus.isConnected() || !m_bus.interface() || m_runtime.isEmpty() || m_socket.isEmpty()) return finish();
    const auto session = m_bus.interface()->serviceOwner(QString::fromLatin1(kSession1));
    const auto compositor = m_bus.interface()->serviceOwner(QString(CompositorNames::service));
    if (!session.isValid() || !compositor.isValid() || session.value().isEmpty() || compositor.value().isEmpty()) {
        // Not published yet: bounded retries; the owner watcher covers later publication.
        static constexpr int delays[] = {250, 500, 1000, 2000, 4000, 8000};
        if (m_attempts < int(std::size(delays))) m_retry.start(delays[m_attempts++]);
        return finish();
    }
    const quint64 pid = peerPid(m_runtime + QLatin1Char('/') + m_socket);
    const QString sessionOwner = session.value();
    const QString compositorOwner = compositor.value();
    m_attachment = std::make_unique<CompositorAttachment>(m_bus, m_runtime, [sessionOwner](const QString &owner) {
        return owner == sessionOwner;
    });
    if (pid == 0 || !m_attachment->attach(sessionOwner, m_socket, PeerExpectation{compositorOwner, pid})) {
        teardown();
        return finish();
    }
    m_transport = std::make_unique<QtNativeLockTransport>(m_bus);
    CompositorAttachment *attachment = m_attachment.get();
    m_monitor = std::make_unique<NativeLockStateMonitor>(*m_transport, [attachment, this, pid](const QString &owner, quint64 daemonPid) {
        const auto identity = attachment->identity();
        return identity && attachment->sameBus(m_bus) && identity->compositorOwner == owner
            && identity->compositorPid == daemonPid && daemonPid == pid;
    });
    connect(m_monitor.get(), &NativeLockStateMonitor::contentMayBeShownChanged, this, [this] { Q_EMIT changed(); });
    connect(attachment, &CompositorAttachment::revoked, this, [this] { Q_EMIT changed(); });
    if (!m_monitor->start()) teardown();
    Q_EMIT changed();
}

} // namespace QindaQt::Link
