// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/services/power_service/adapters/upstream_composition.h>
#include <qindaqt/services/power_service/resident_power_service.h>
#include <qindaqt/services/power_service/source_profile_policy.h>
#include <qindaqt/services/power_service/adapters/native_profile_authority.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/power_service/critical_battery_policy.h>
#include <qindaqt/services/power_service/adapters/critical_notification_adapter.h>
#include <qindaqt/services/session_actions/session_actions_client.h>
#include <qindaqt/services/power_service/lid_policy.h>
#include <qindaqt/services/power_service/adapters/logind_lid_authority.h>
#ifdef QINDAQT_PRIVATE_LID_FIXTURE
#include <unistd.h>
#endif

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
        {QStringLiteral("idle-policy"),
         QStringLiteral("Shared idle consumers: off or native-exclusive (explicit cutover)."),
         QStringLiteral("mode"), QStringLiteral("off")},
        {QStringLiteral("lid-policy"),
         QStringLiteral("Native lid handling: off or native-exclusive (explicit cutover)."),
         QStringLiteral("mode"), QStringLiteral("off")},
        {QStringLiteral("critical-policy"),
         QStringLiteral("Critical battery countdown: off or native-exclusive (explicit cutover)."),
         QStringLiteral("mode"), QStringLiteral("off")},
        {QStringLiteral("profile-policy"),
         QStringLiteral("Automatic source profiles: off or native-exclusive (explicit cutover)."),
         QStringLiteral("mode"), QStringLiteral("off")},
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
    const QString profilePolicy = parser.value(QStringLiteral("profile-policy"));
    if (profilePolicy != QStringLiteral("off")
        && profilePolicy != QStringLiteral("native-exclusive")) {
        qCritical("Power1 rejected unknown profile policy mode");
        return 1;
    }
    const QString criticalPolicy = parser.value(QStringLiteral("critical-policy"));
    if (criticalPolicy != QStringLiteral("off") && criticalPolicy != QStringLiteral("native-exclusive")) {
        qCritical("Power1 rejected unknown critical policy mode");
        return 1;
    }
    const QString backlightRoot = parser.value(QStringLiteral("backlight-root"));
    const QString idlePolicy = parser.value(QStringLiteral("idle-policy"));
    if (idlePolicy != QStringLiteral("off") && idlePolicy != QStringLiteral("native-exclusive")) {
        qCritical("Power1 rejected unknown idle policy mode");
        return 1;
    }
    const QString lidPolicy = parser.value(QStringLiteral("lid-policy"));
    if (lidPolicy != QStringLiteral("off") && lidPolicy != QStringLiteral("native-exclusive")) {
        qCritical("Power1 rejected unknown lid policy mode");
        return 1;
    }

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
    // AGENT-GUARD: NameAlreadyOwned means a live sibling already provides
    // Power1 -- not a failure of this process. Exiting nonzero here would
    // have systemd's Restart=on-failure retry a start that can only ever
    // fail the same way again while the sibling lives, looping until
    // StartLimitBurst; exit 0 lets the unit settle once, with the reason on
    // the record.
    if (status == PowerServiceStartStatus::NameAlreadyOwned) {
        qInfo("Power1 startup stopped: org.qindaqt.Power1 is already owned "
              "by a live sibling process");
        return 0;
    }
    if (status != PowerServiceStartStatus::Started) {
        qCritical("Power1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }

    // Explicit composition is dormant in packaged production until the final
    // PowerDevil cutover. The same assembly runs against private fixture buses.
    using namespace QindaQt::Services::SettingsClient;
    std::unique_ptr<QtSettingsTransport> settingsTransport;
    std::unique_ptr<SettingsClient> settings;
    std::unique_ptr<SourceProfilePolicy> sourcePolicy;
    std::unique_ptr<Upstream::NativeProfileAuthority> authority;
    std::unique_ptr<QindaQt::Services::SessionActions::SessionActionsClient> actions;
    std::unique_ptr<Upstream::CriticalNotificationAdapter> criticalNotification;
    std::unique_ptr<CriticalBatteryPolicy> criticalBattery;
    std::unique_ptr<QindaQt::Services::SessionActions::SessionActionsClient> lidActions;
    std::unique_ptr<Upstream::LogindLidAuthority> lidHandling;
    std::unique_ptr<LidPolicy> nativeLid;
    const bool profilesEnabled = profilePolicy == QStringLiteral("native-exclusive");
    const bool criticalEnabled = criticalPolicy == QStringLiteral("native-exclusive");
    const bool lidEnabled = lidPolicy == QStringLiteral("native-exclusive");
    if (profilesEnabled || criticalEnabled || lidEnabled) {
        settingsTransport = std::make_unique<QtSettingsTransport>(sessionConnection);
        QStringList keys;
        if (profilesEnabled) keys.append(SourceProfilePolicy::settingsKeys());
        if (criticalEnabled) keys.append(CriticalBatteryPolicy::settingsKeys());
        if (lidEnabled) keys.append(LidPolicy::settingsKeys());
        settings = std::make_unique<SettingsClient>(*settingsTransport, keys);
        if (profilesEnabled) sourcePolicy = std::make_unique<SourceProfilePolicy>(*service.coordinator(), *settings);
        if (criticalEnabled) {
            actions = std::make_unique<QindaQt::Services::SessionActions::SessionActionsClient>(sessionConnection, upstreamConnection);
            criticalNotification = std::make_unique<Upstream::CriticalNotificationAdapter>(sessionConnection);
            criticalBattery = std::make_unique<CriticalBatteryPolicy>(*service.coordinator(), *settings, *criticalNotification, *actions);
            actions->start();
        }
        if (lidEnabled) {
            // Installed resident has no CLI/environment credential override.
            // Only the noninstalled test target injects its private actor UID.
#ifdef QINDAQT_PRIVATE_LID_FIXTURE
            const quint32 expectedLogindUid = quint32(::getuid());
#else
            constexpr quint32 expectedLogindUid = 0;
#endif
            lidActions = std::make_unique<QindaQt::Services::SessionActions::SessionActionsClient>(sessionConnection, upstreamConnection);
            lidHandling = std::make_unique<Upstream::LogindLidAuthority>(sessionConnection, upstreamConnection, expectedLogindUid);
            nativeLid = std::make_unique<LidPolicy>(*service.coordinator(), *settings, *lidHandling, *lidActions);
            QObject::connect(&application, &QCoreApplication::aboutToQuit, nativeLid.get(),
                [policy = nativeLid.get()] { policy->setNativeAuthority(false); });
        }
        authority = std::make_unique<Upstream::NativeProfileAuthority>(sessionConnection, true);
        if (sourcePolicy) QObject::connect(authority.get(), &Upstream::NativeProfileAuthority::admissionChanged,
                         sourcePolicy.get(), &SourceProfilePolicy::setNativeAuthority);
        if (criticalBattery) QObject::connect(authority.get(), &Upstream::NativeProfileAuthority::admissionChanged,
                         criticalBattery.get(), &CriticalBatteryPolicy::setNativeAuthority);
        if (nativeLid) QObject::connect(authority.get(), &Upstream::NativeProfileAuthority::admissionChanged,
                         nativeLid.get(), &LidPolicy::setNativeAuthority);
        authority->start();
        if (sourcePolicy) sourcePolicy->setNativeAuthority(authority->admitted());
        if (criticalBattery) criticalBattery->setNativeAuthority(authority->admitted());
        if (nativeLid) nativeLid->setNativeAuthority(authority->admitted());
        if (!settings->start()) {
            qCritical("Power1 source profile Settings1 observation failed");
        }
    }

    std::unique_ptr<Upstream::NativeProfileAuthority> idleAdmission;
    if (idlePolicy == QStringLiteral("native-exclusive")) {
        idleAdmission = std::make_unique<Upstream::NativeProfileAuthority>(sessionConnection, true);
        QObject::connect(idleAdmission.get(), &Upstream::NativeProfileAuthority::admissionChanged,
                         &service, &ResidentPowerService::setNativeIdleAdmission);
        idleAdmission->start();
        service.setNativeIdleAdmission(idleAdmission->admitted());
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &ResidentPowerService::stop);
    return QCoreApplication::exec();
}
