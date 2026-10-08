// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_clipboard_lock_observer.h"

#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>

#include <algorithm>
#include <array>
#include <utility>

namespace QindaQt::Services::Clipboard {
using Platform::Compositor::CompositorAttachment;
using SessionLockState::NativeLockStateMonitor;
using SessionLockState::QtNativeLockTransport;

class NativeClipboardLockObserver::Private final {
public:
    Private(QDBusConnection connection, qint64 pid, QString runtime, QString socket)
        : bus(std::move(connection)), compositorPid(pid),
          runtimeDirectory(std::move(runtime)), socketBasename(std::move(socket)) {
        startup.setSingleShot(true);
    }
    QDBusConnection bus;
    qint64 compositorPid;
    QString runtimeDirectory, socketBasename, sessionOwner;
    bool started = false, retired = false;
    int retry = 0;
    QTimer startup;
    QElapsedTimer startupWindow;
    // AGENT-GUARD: reverse destruction and stop() invalidate the receipt monitor
    // while its borrowed attachment, transport and bus still exist.
    std::unique_ptr<CompositorAttachment> attachment;
    std::unique_ptr<QtNativeLockTransport> transport;
    std::unique_ptr<NativeLockStateMonitor> monitor;
};

NativeClipboardLockObserver::NativeClipboardLockObserver(
    QDBusConnection bus, qint64 compositorPid, QString runtime, QString socket, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(std::move(bus), compositorPid,
          std::move(runtime), std::move(socket))) {
    connect(&d->startup, &QTimer::timeout, this, &NativeClipboardLockObserver::probeInitialOwner);
}
NativeClipboardLockObserver::~NativeClipboardLockObserver() { stop(); }

bool NativeClipboardLockObserver::start()
{
    if (d->retired) return false;
    if (d->started) return true;
    if (QThread::currentThread() != thread() || d->compositorPid <= 1
        || !d->bus.isConnected() || !d->bus.interface()) {
        stop();
        return false;
    }
    d->started = true;
    d->startupWindow.start();
    probeInitialOwner();
    return !d->retired;
}

void NativeClipboardLockObserver::probeInitialOwner()
{
    if (!d->started || d->retired || d->attachment || d->monitor) return;
    if (!d->bus.isConnected() || !d->bus.interface()
        || d->startupWindow.elapsed() >= 30'000) {
        stop();
        return;
    }
    d->bus.interface()->setTimeout(250);
    const auto session = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    const auto compositor = d->bus.interface()->serviceOwner(QString(CompositorNames::service));
    if (!session.isValid() || !compositor.isValid()
        || session.value().isEmpty() || compositor.value().isEmpty()) {
        // AGENT-NOTE: supervisor starts resident services before publishing
        // Session1 (ADR-0358). No owner has been selected here. One bounded
        // timer observes initial publication only; repeated start() cannot
        // allocate more probes, activate a service or reset its deadline.
        static constexpr std::array delays{50, 100, 250, 500, 1000, 2000, 4000, 8000, 8000, 6000};
        if (d->retry < int(delays.size()) && d->startupWindow.elapsed() < 30'000) {
            const auto remaining = static_cast<int>(30'000 - d->startupWindow.elapsed());
            d->startup.start(std::min(delays[static_cast<std::size_t>(d->retry++)],
                                     std::max(1, remaining)));
        } else {
            stop();
        }
        return;
    }
    if (d->startupWindow.elapsed() >= 30'000) {
        stop();
        return;
    }
    // Pin once before attachment/receipt admission. Invalid socket/PID proof or
    // subsequent loss is terminal; a replacement is never a startup retry.
    d->startup.stop();
    d->sessionOwner = session.value();
    d->attachment = std::make_unique<CompositorAttachment>(
        d->bus, d->runtimeDirectory, [this](const QString &owner) {
            if (d->retired || !d->bus.interface() || owner != d->sessionOwner) return false;
            const auto current = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
            return current.isValid() && current.value() == owner;
        });
    if (!d->attachment->attach(d->sessionOwner, d->socketBasename,
            Platform::Compositor::PeerExpectation{
                compositor.value(), static_cast<quint64>(d->compositorPid)})) {
        stop();
        return;
    }
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
        // Retire before publishing denial. No attachment is rebound here.
        d->retired = true;
        if (d->monitor) d->monitor->stop();
        Q_EMIT contentMayBeShownChanged(false);
    });
    if (!d->monitor->start()) stop();
}

void NativeClipboardLockObserver::stop()
{
    d->retired = true;
    d->started = false;
    d->startup.stop();
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
