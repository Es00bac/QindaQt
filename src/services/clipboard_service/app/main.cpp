// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>
#include <qindaqt/services/clipboard_wayland_adapter/production_clipboard_wayland_adapter.h>
#include <qindaqt/services/session_lock_state/qt_session_lock_transport.h>
#include <qindaqt/services/session_lock_state/session_lock_state_monitor.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include "clipboard_history_consent.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

#include <memory>

using namespace QindaQt::Services;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-clipboard-host"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical("Clipboard1 requires a connected session bus");
        return 1;
    }
    if (!bus.connect(QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
                     QStringLiteral("org.freedesktop.DBus.Local"),
                     QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("Clipboard1 could not bind constructing-bus lifetime");
        return 1;
    }

    auto adapter = ClipboardWayland::makeProductionClipboardWaylandAdapter();
    const ClipboardWayland::StartStatus adapterStatus = adapter->start();
    const qint64 compositorPid = adapterStatus == ClipboardWayland::StartStatus::Started
        ? adapter->peerProcessId() : 0;
    const quint64 epoch = static_cast<quint64>(
        qHash(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    Clipboard::ResidentClipboardService service(std::move(adapter), bus, {}, epoch);

    SettingsClient::QtSettingsTransport settingsTransport(bus);
    SettingsClient::SettingsClient settingsClient(
        settingsTransport, {QStringLiteral("services.clipboardHistory")});
    QObject::connect(&settingsClient, &SettingsClient::SettingsClient::snapshotChanged,
                     &service, [&settingsClient, &service] {
        service.host()->setHistoryOptIn(
            Clipboard::hasExplicitHistoryConsent(settingsClient.snapshot()));
    });
    QObject::connect(&settingsClient, &SettingsClient::SettingsClient::stateChanged,
                     &service, [&settingsClient, &service] {
        if (settingsClient.state() != SettingsClient::ClientState::Ready) {
            service.host()->setHistoryOptIn(false);
        }
    });

    std::unique_ptr<SessionLockState::QtSessionLockTransport> lockTransport;
    std::unique_ptr<SessionLockState::SessionLockStateMonitor> lockMonitor;
    if (compositorPid > 0) {
        lockTransport = std::make_unique<SessionLockState::QtSessionLockTransport>(bus);
        lockMonitor = std::make_unique<SessionLockState::SessionLockStateMonitor>(
            *lockTransport, compositorPid);
        QObject::connect(lockMonitor.get(),
                         &SessionLockState::SessionLockStateMonitor::contentMayBeShownChanged,
                         &service, [&service](bool allowed) { service.host()->setUnlocked(allowed); });
        QString lockError;
        if (!lockMonitor->start(&lockError)) {
            qWarning("Clipboard1 lock authority unavailable");
        }
    }

    if (service.start() != Clipboard::ServiceStartStatus::Started) {
        qCritical("Clipboard1 service registration failed");
        return 2;
    }
    QString settingsError;
    if (!settingsClient.start(&settingsError)) {
        qWarning("Clipboard1 Settings1 gate unavailable");
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &service, &Clipboard::ResidentClipboardService::stop);
    return QCoreApplication::exec();
}
