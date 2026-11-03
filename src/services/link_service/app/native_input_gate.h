// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_input/input_sink.h>

#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

#include <memory>

namespace QindaQt::Platform::Compositor { class CompositorAttachment; }
namespace QindaQt::Services::SessionLockState {
class QtNativeLockTransport;
class NativeLockStateMonitor;
}

namespace QindaQt::Link {

// Production InputGate: open only while an authenticated native compositor
// receipt says the session is Unlocked (ADR-0320's pattern, as used by the
// clipboard host). Anything else is closed: no compositor, no Session1,
// Locking, Locked, Unknown, or an attachment that cannot be proven.
//
// AGENT-NOTE: the expected compositor PID here is the kernel peer of the
// Wayland socket, checked against the bus owner of the compositor name; the
// clipboard host takes it from its Wayland adapter and the shell from the
// supervisor. A supervisor-provisioned PID would be stronger; replace
// peerPid() when the service gets one.
// Threading: GUI thread. Ownership: owns its attachment, transport, monitor.
class NativeInputGate final : public InputGate {
    Q_OBJECT
public:
    NativeInputGate(QDBusConnection bus, QString runtimeDirectory, QString socketBasename, QObject *parent = nullptr);
    ~NativeInputGate() override;
    [[nodiscard]] bool open() const override;

private:
    void rebind();
    void teardown();

    QDBusConnection m_bus;
    QString m_runtime, m_socket;
    QDBusServiceWatcher m_watcher;
    QTimer m_retry;
    int m_attempts = 0;
    std::unique_ptr<Platform::Compositor::CompositorAttachment> m_attachment;
    std::unique_ptr<Services::SessionLockState::QtNativeLockTransport> m_transport;
    std::unique_ptr<Services::SessionLockState::NativeLockStateMonitor> m_monitor;
};

} // namespace QindaQt::Link
