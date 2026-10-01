// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_notification_lock_observer.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QThread>
#include <utility>

namespace QindaQt::Shell {
using Platform::Compositor::CompositorAttachment;
using Services::SessionLockState::NativeLockStateMonitor;
using Services::SessionLockState::QtNativeLockTransport;

class NativeNotificationLockObserver::Private final {
public:
    Private(QDBusConnection connection, qint64 pid, QString runtime, QString socket)
        : bus(std::move(connection)), compositorPid(pid),
          runtimeDirectory(std::move(runtime)), socketBasename(std::move(socket)) {}
    QDBusConnection bus;
    qint64 compositorPid;
    QString runtimeDirectory, socketBasename, sessionOwner;
    // AGENT-GUARD: monitor admission borrows attachment, transport and bus;
    // declaration order and stop() both keep them alive through invalidation.
    std::unique_ptr<CompositorAttachment> attachment;
    std::unique_ptr<QtNativeLockTransport> transport;
    std::unique_ptr<NativeLockStateMonitor> monitor;
};

NativeNotificationLockObserver::NativeNotificationLockObserver(
        QDBusConnection bus, qint64 pid, QString runtime, QString socket, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(std::move(bus), pid,
          std::move(runtime), std::move(socket))) {}
NativeNotificationLockObserver::~NativeNotificationLockObserver() { stop(); }

bool NativeNotificationLockObserver::start(QString *error)
{
    if (d->monitor) return true;
    const auto fail = [this, error](const QString &reason) {
        stop();
        if (error) *error = reason;
        return false;
    };
    if (QThread::currentThread() != thread() || d->compositorPid <= 1 ||
            !d->bus.isConnected() || !d->bus.interface())
        return fail(QStringLiteral("native notification lock authority is unavailable"));
    d->bus.interface()->setTimeout(250);
    const auto session = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    const auto compositor = d->bus.interface()->serviceOwner(QString(CompositorNames::service));
    if (!session.isValid() || !compositor.isValid() || session.value().isEmpty() ||
            compositor.value().isEmpty())
        return fail(QStringLiteral("native session or compositor owner is unavailable"));
    d->sessionOwner = session.value();
    d->attachment = std::make_unique<CompositorAttachment>(
        d->bus, d->runtimeDirectory, [this](const QString &owner) {
            if (!d->bus.interface() || owner != d->sessionOwner) return false;
            const auto current = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
            return current.isValid() && current.value() == owner;
        });
    if (!d->attachment->attach(d->sessionOwner, d->socketBasename,
            Platform::Compositor::PeerExpectation{compositor.value(), quint64(d->compositorPid)}))
        return fail(QStringLiteral("native notification ordinary attachment was refused"));
    d->transport = std::make_unique<QtNativeLockTransport>(d->bus);
    d->monitor = std::make_unique<NativeLockStateMonitor>(*d->transport,
        [this](const QString &owner, quint64 pid) {
            const auto identity = d->attachment->identity();
            return identity && d->attachment->sameBus(d->bus) &&
                identity->compositorOwner == owner && identity->compositorPid == pid;
        });
    connect(d->monitor.get(), &NativeLockStateMonitor::contentMayBeShownChanged,
            this, &NativeNotificationLockObserver::contentMayBeShownChanged);
    connect(d->attachment.get(), &CompositorAttachment::revoked, this, [this] {
        if (d->monitor) d->monitor->refresh();
    });
    QString detail;
    if (!d->monitor->start(&detail)) return fail(detail);
    if (error) error->clear();
    return true;
}

void NativeNotificationLockObserver::stop()
{
    if (d->monitor) d->monitor->stop();
    d->monitor.reset();
    d->transport.reset();
    if (d->attachment) d->attachment->revoke();
    d->attachment.reset();
    d->sessionOwner.clear();
}
bool NativeNotificationLockObserver::contentMayBeShown() const
{
    return d->monitor && d->monitor->contentMayBeShown();
}
}
