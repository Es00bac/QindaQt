// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_clipboard_lock_observer.h"

#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>

#include <QtCore/QThread>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>

#include <utility>

namespace QindaQt::Services::Clipboard {
using Platform::Compositor::CompositorAttachment;
using SessionLockState::NativeLockStateMonitor;
using SessionLockState::QtNativeLockTransport;

class NativeClipboardLockObserver::Private final {
public:
    Private(QDBusConnection connection, qint64 pid, QString runtime, QString socket)
        : bus(std::move(connection)), compositorPid(pid),
          runtimeDirectory(std::move(runtime)), socketBasename(std::move(socket)) {}
    QDBusConnection bus;
    qint64 compositorPid;
    QString runtimeDirectory, socketBasename, sessionOwner;
    bool retired = false;
    // AGENT-GUARD: reverse destruction and stop() invalidate the receipt monitor
    // while its borrowed attachment, transport and bus still exist.
    std::unique_ptr<CompositorAttachment> attachment;
    std::unique_ptr<QtNativeLockTransport> transport;
    std::unique_ptr<NativeLockStateMonitor> monitor;
};

NativeClipboardLockObserver::NativeClipboardLockObserver(
    QDBusConnection bus, qint64 compositorPid, QString runtime, QString socket, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(std::move(bus), compositorPid,
          std::move(runtime), std::move(socket))) {}
NativeClipboardLockObserver::~NativeClipboardLockObserver() { stop(); }

bool NativeClipboardLockObserver::start()
{
    if (d->retired) return false;
    if (d->monitor) return true;
    const auto fail = [this] { stop(); return false; };
    if (QThread::currentThread() != thread() || d->compositorPid <= 1
        || !d->bus.isConnected() || !d->bus.interface()) return fail();
    d->bus.interface()->setTimeout(250);
    const auto session = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    const auto compositor = d->bus.interface()->serviceOwner(QString(CompositorNames::service));
    if (!session.isValid() || !compositor.isValid()
        || session.value().isEmpty() || compositor.value().isEmpty()) return fail();
    // First-session selection is explicit local policy, not executable
    // attestation. Independent socket/kernel and bus owner/PID proof below
    // supplies compositor authority; a facade payload never supplies a PID.
    d->sessionOwner = session.value();
    d->attachment = std::make_unique<CompositorAttachment>(
        d->bus, d->runtimeDirectory, [this](const QString &owner) {
            if (d->retired || !d->bus.interface() || owner != d->sessionOwner) return false;
            const auto current = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
            return current.isValid() && current.value() == owner;
        });
    if (!d->attachment->attach(d->sessionOwner, d->socketBasename,
            Platform::Compositor::PeerExpectation{
                compositor.value(), static_cast<quint64>(d->compositorPid)})) return fail();
    d->transport = std::make_unique<QtNativeLockTransport>(d->bus);
    d->monitor = std::make_unique<NativeLockStateMonitor>(*d->transport,
        [this](const QString &owner, quint64 pid) {
            const auto identity = d->attachment->identity();
            return !d->retired && identity && d->attachment->sameBus(d->bus)
                && identity->compositorOwner == owner && identity->compositorPid == pid
                && pid == static_cast<quint64>(d->compositorPid);
        });
    connect(d->monitor.get(), &NativeLockStateMonitor::contentMayBeShownChanged,
            this, [this](bool allowed) {
        Q_EMIT contentMayBeShownChanged(allowed && contentMayBeShown());
    });
    connect(d->attachment.get(), &CompositorAttachment::revoked, this, [this] {
        // No attachment is rebound here. Retire before publishing denial, so
        // reentrant getters cannot revive a cached unlocked receipt.
        d->retired = true;
        if (d->monitor) d->monitor->stop();
        Q_EMIT contentMayBeShownChanged(false);
    });
    return d->monitor->start() || fail();
}

void NativeClipboardLockObserver::stop()
{
    d->retired = true;
    if (d->monitor) d->monitor->stop();
    d->monitor.reset();
    d->transport.reset();
    if (d->attachment) d->attachment->revoke();
    d->attachment.reset();
    d->sessionOwner.clear();
    Q_EMIT contentMayBeShownChanged(false);
}

bool NativeClipboardLockObserver::contentMayBeShown() const
{
    return !d->retired && d->monitor && d->monitor->contentMayBeShown();
}

} // namespace QindaQt::Services::Clipboard
