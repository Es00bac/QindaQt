// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>
#include <qindaqt/services/clipboard_wayland_adapter/production_clipboard_wayland_adapter.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include "clipboard_history_consent.h"
#include "native_clipboard_lock_observer.h"

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
    // AGENT-GUARD: ordinary adapter proof supplies PID; environment only selects
    // the runtime/socket. Observer is declared before the host which borrows it,
    // and no cached adapter PID/availability is called live socket authority.
    Clipboard::NativeClipboardLockObserver lockObserver(
        bus, compositorPid, qEnvironmentVariable("XDG_RUNTIME_DIR"),
        qEnvironmentVariable("WAYLAND_DISPLAY"));
    Clipboard::ResidentClipboardService service(std::move(adapter), bus, {}, epoch,
        [&lockObserver] { return lockObserver.contentMayBeShown(); });
    QObject::connect(&lockObserver, &Clipboard::NativeClipboardLockObserver::contentMayBeShownChanged,
                     &service, [&service](bool allowed) { service.host()->setUnlocked(allowed); });

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

    const Clipboard::ServiceStartStatus status = service.start();
    // AGENT-GUARD: NameAlreadyOwned means a live sibling already provides
    // Clipboard1 -- not a failure of this process. Exiting nonzero here
    // would have systemd's Restart=on-failure retry a start that can only
    // ever fail the same way again while the sibling lives, looping until
    // StartLimitBurst; exit 0 lets the unit settle once, with the reason on
    // the record.
    if (status == Clipboard::ServiceStartStatus::NameAlreadyOwned) {
        // AGENT-GUARD: this is qWarning on purpose. The clipboard boundary
        // gate (tests/services/clipboard_service/check_boundary.cmake) permits
        // only the warning and critical severities anywhere in the clipboard
        // production tree, because this is the one service whose log lines can
        // carry what the user copied. The message below has no payload in it,
        // but the gate is categorical on purpose — a rule with exceptions is
        // not a boundary. The gate reads whole files, comments included, so do
        // not name the forbidden calls here either.
        qWarning("Clipboard1 startup stopped: org.qindaqt.Clipboard1 is already "
                 "owned by a live sibling process");
        return 0;
    }
    if (status != Clipboard::ServiceStartStatus::Started) {
        qCritical("Clipboard1 service registration failed");
        return 2;
    }
    if (!lockObserver.start()) {
        qWarning("Clipboard1 native lock authority unavailable");
    }
    QString settingsError;
    if (!settingsClient.start(&settingsError)) {
        qWarning("Clipboard1 Settings1 gate unavailable");
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &service, [&service, &lockObserver] {
        service.stop();
        lockObserver.stop();
    });
    return QCoreApplication::exec();
}
