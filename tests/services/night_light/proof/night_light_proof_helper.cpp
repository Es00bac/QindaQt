// SPDX-License-Identifier: LGPL-3.0-or-later

// Private proof helper for the night light lane (ADR-0136). Two modes:
//
//   write <kwinrc> <knighttimerc>   Writes Active=true, Mode=Constant,
//                                   NightTemperature=3400 through the
//                                   production QtConfigNightLightPort and
//                                   prints the write outcome.
//   hold-inhibit <seconds>          Connects to the session bus, calls
//                                   org.kde.KWin.NightLight.inhibit(), keeps
//                                   the connection open for the given
//                                   seconds, then exits WITHOUT uninhibiting
//                                   so the proof can observe that the lock
//                                   dies with the caller's connection.
#include <qindaqt/services/night_light/night_light_config_port.h>

#include <QCoreApplication>
#include <QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

using QindaQt::Services::NightLight::QtConfigNightLightPort;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (argc >= 4 && argv[1] == QLatin1String("write")) {
        // KConfigWatcher in the daemons matches the writer's component name
        // against the config's own component; announce as "kwin" so the
        // notify reaches KWin's watcher for its kwinrc.
        QCoreApplication::setApplicationName(QStringLiteral("kwin"));
        QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
        QtConfigNightLightPort port(QString::fromUtf8(argv[2]),
                                    QString::fromUtf8(argv[3]));
        QindaQt::Services::NightLight::NightLightSettings settings;
        settings.output.active = true;
        settings.output.mode = QindaQt::Services::NightLight::Mode::Constant;
        settings.output.nightTemperatureKelvin = 3400;
        const auto result = port.write(settings);
        QCoreApplication::exit(result.outcome
                                       == QindaQt::Services::NightLight::
                                          NightLightConfigPort::WriteOutcome::Applied
                                   ? 0
                                   : 1);
        return 0;
    }

    if (argc >= 3 && argv[1] == QLatin1String("hold-inhibit")) {
        const int seconds = QByteArray(argv[2]).toInt();
        const QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            qWarning("no session bus");
            return 2;
        }
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin.NightLight"),
            QStringLiteral("/org/kde/KWin/NightLight"),
            QStringLiteral("org.kde.KWin.NightLight"),
            QStringLiteral("inhibit"));
        auto *watcher =
            new QDBusPendingCallWatcher(bus.asyncCall(call), &app);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &app,
                         [&app, watcher, seconds](QDBusPendingCallWatcher *) {
                             watcher->deleteLater();
                             QDBusPendingReply<uint> reply = *watcher;
                             if (reply.isError()) {
                                 qWarning("inhibit failed: %s",
                                          qPrintable(reply.error().message()));
                                 QCoreApplication::exit(3);
                                 return;
                             }
                             qInfo("inhibited with cookie %u; holding for %d s",
                                   reply.value(), seconds);
                             QTimer::singleShot(seconds * 1000, &app, [&app] {
                                 // AGENT-NOTE: Deliberately no uninhibit():
                                 // the proof observes the lock disappearing
                                 // when this process (the caller) exits.
                                 qInfo("caller exiting; lock must die with "
                                       "the connection");
                                 QCoreApplication::exit(0);
                             });
                         });
        return QCoreApplication::exec();
    }

    qWarning("usage: helper write <kwinrc> <knighttimerc> | "
             "hold-inhibit <seconds>");
    return 64;
}
