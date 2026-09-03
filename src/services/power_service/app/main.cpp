// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/services/power_service/adapters/upstream_composition.h>
#include <qindaqt/services/power_service/resident_power_service.h>

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusConnection>

#include <memory>

using namespace QindaQt::Power;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-power-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    // AGENT-NOTE: the bare binary defaults to the PB-1 unavailable upstream so
    // an unconfigured execution never contacts a host bus; the packaged
    // descriptor and user unit pass --upstream=production explicitly.
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaQt resident Power1 service"));
    parser.addOptions({
        {QStringLiteral("upstream"),
         QStringLiteral("Upstream collaborator mode: production or unavailable."),
         QStringLiteral("mode"),
         QStringLiteral("unavailable")},
        {QStringLiteral("backlight-root"),
         QStringLiteral("Injected backlight sysfs root for production mode."),
         QStringLiteral("path"),
         QString::fromLatin1(Upstream::kSysfsBacklightDefaultRoot)},
    });
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);

    const auto mode =
        Upstream::parseUpstreamMode(parser.value(QStringLiteral("upstream")));
    if (!mode.has_value()) {
        qCritical("Power1 rejected unknown upstream mode '%s'",
                  qPrintable(parser.value(QStringLiteral("upstream"))));
        return 1;
    }
    const QString backlightRoot = parser.value(QStringLiteral("backlight-root"));

    QDBusConnection sessionConnection = QDBusConnection::sessionBus();
    // AGENT-GUARD: This activated process belongs to exactly the bus that
    // constructed it. Bus replacement must terminate the process; reconnecting
    // would expose stale collaborator/epoch state under a new authority
    // lineage.
    if (!sessionConnection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"),
            QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("Power1 could not bind constructing-bus lifetime");
        return 1;
    }

    // The upstream bus is opened only for production mode; the unavailable
    // mode never constructs a system-bus connection at all.
    const QDBusConnection upstreamConnection =
        mode == Upstream::UpstreamMode::Production
        ? QDBusConnection::systemBus()
        : QDBusConnection(QStringLiteral("power1-upstream-unused"));
    Upstream::UpstreamComposition composition =
        Upstream::composeUpstream(mode.value(), upstreamConnection, backlightRoot);
    ResidentPowerService service(std::move(composition.battery),
                                 std::move(composition.profiles),
                                 std::move(composition.session), sessionConnection);
    const PowerServiceStartStatus status = service.start();
    if (status != PowerServiceStartStatus::Started) {
        qCritical("Power1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &ResidentPowerService::stop);
    return QCoreApplication::exec();
}
