// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>

#include <QtDBus/QDBusConnection>

#include <qindaqt/services/xembed_tray_proxy/tray_proxy_coordinator.h>

#include "../src/x11/xcb_tray_backend.h"

using namespace QindaQt::XEmbedTray;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QStringLiteral("qindaqt-xembed-tray-proxy"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "QindaQt XEmbed to StatusNotifierItem tray proxy (Wine/Proton tray "
        "icons). Owns the _NET_SYSTEM_TRAY_S<n> selection on the session's "
        "X display and publishes each docked icon as a StatusNotifierItem."));
    const QCommandLineOption displayOption(
        QStringLiteral("display"),
        QStringLiteral("X display name; defaults to the DISPLAY environment "
                       "variable."),
        QStringLiteral("name"));
    const QCommandLineOption busAddressOption(
        QStringLiteral("bus-address"),
        QStringLiteral("D-Bus address to serve; defaults to the session bus."),
        QStringLiteral("address"));
    const QCommandLineOption waitDisplayOption(
        QStringLiteral("wait-display"),
        QStringLiteral("Seconds to wait for the X display to appear before "
                       "giving up (0 = no wait)."),
        QStringLiteral("seconds"), QStringLiteral("30"));
    parser.addOptions({displayOption, busAddressOption, waitDisplayOption});
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);

    XcbTrayBackend backend(parser.value(displayOption));
    TrayProxyCoordinator::ConnectionFactory connectionFactory;
    const QString busAddress = parser.value(busAddressOption);
    if (busAddress.isEmpty()) {
        connectionFactory = [](const QString &name) {
            return QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                 name);
        };
    } else {
        connectionFactory = [&busAddress](const QString &name) {
            return QDBusConnection::connectToBus(busAddress, name);
        };
    }

    // XWayland can legitimately lag the session's start; bound the wait so a
    // display-less session exits cleanly instead of racing forever.
    bool ok = false;
    int waitedSeconds = 0;
    const int waitDisplaySeconds =
        parser.value(waitDisplayOption).toInt();
    while (!ok) {
        ok = backend.start();
        if (ok || waitedSeconds >= waitDisplaySeconds) {
            break;
        }
        QThread::sleep(2);
        waitedSeconds += 2;
    }
    if (!ok) {
        qCritical("XEmbed tray proxy: no usable X display; exiting");
        return 1;
    }

    TrayProxyCoordinator coordinator(&backend, connectionFactory);
    if (!coordinator.start()) {
        qCritical("XEmbed tray proxy: startup failed");
        return 1;
    }

    // AGENT-GUARD: This process belongs to exactly the bus that constructed
    // it. Bus replacement must terminate the process; reconnecting would
    // publish stale items under a new authority lineage.
    QDBusConnection lifetimeConnection =
        connectionFactory(QStringLiteral("qindaqt-xembed-tray-lifetime"));
    if (!lifetimeConnection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"),
            QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("XEmbed tray proxy: could not bind constructing-bus lifetime");
        return 1;
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &coordinator,
                     &TrayProxyCoordinator::stop);
    return QCoreApplication::exec();
}
