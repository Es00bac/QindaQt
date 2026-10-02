// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "profile_policy_bus.h"
#include "fake_upower_service.h"
#include "profile_settings_source.h"
#include "critical_action_wire.h"
#include "critical_notification_fault.h"
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_service/resident_settings_service.h>
#include <qindaqt/services/notification_host/resident_notification_host.h>
#include <qindaqt/services/notification_host/qt_deadline_scheduler.h>
#include <qindaqt/services/notification_presentation/wire_contract.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <memory>
namespace QindaQt::Tests {
struct CriticalRuntime {
    ProfilePolicyBus bus;
    QDBusConnection settingsBus{QStringLiteral("unused-critical-settings")};
    QDBusConnection notificationBus{QStringLiteral("unused-critical-notifications")};
    QDBusConnection actionBus{QStringLiteral("unused-critical-actions")};
    QDBusConnection upowerBus{QStringLiteral("unused-critical-upower")};
    QDBusConnection clientBus{QStringLiteral("unused-critical-client")};
    QDBusConnection presenterBus{QStringLiteral("unused-critical-presenter")};
    QDBusConnection legacyBus{QStringLiteral("unused-critical-legacy")};
    Services::Notifications::SteadyNotificationClock clock;
    Services::NotificationHost::QtNotificationDeadlineScheduler scheduler;
    const Services::NotificationPresentation::PresentationAccessToken token =
        Services::NotificationPresentation::PresentationAccessToken::generate();
    std::unique_ptr<Services::NotificationHost::ResidentNotificationHost> host;
    std::unique_ptr<CriticalNotificationFault> fault;
    std::unique_ptr<Services::SettingsService::ResidentSettingsService> service;
    std::unique_ptr<ProfileSettingsSource> adversarial;
    std::unique_ptr<CriticalActionWire> action;
    std::unique_ptr<FakeUpowerService> upower;
    std::unique_ptr<Services::SettingsClient::QtSettingsTransport> transport;
    std::unique_ptr<Services::SettingsClient::SettingsClient> settings;
    std::unique_ptr<Power::QtPowerTransport> powerTransport;
    std::unique_ptr<Power::PowerClient> power;
    QProcess process;
    bool presenterRegistered = false;
    ~CriticalRuntime() {
        process.terminate();
        if (!process.waitForFinished(2000)) { process.kill(); process.waitForFinished(); }
    }
    bool start(bool enabled = true, bool legacy = false, bool malicious = false, bool notificationFault = false) {
        if (!bus.start()) return false;
        settingsBus = bus.open(); notificationBus = bus.open(); actionBus = bus.open();
        upowerBus = bus.open(); clientBus = bus.open(); presenterBus = bus.open(); legacyBus = bus.open();
        if (legacy && !legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement"))) return false;
        QString error;
        if (malicious) {
            adversarial = std::make_unique<ProfileSettingsSource>(settingsBus);
            adversarial->values = {{QStringLiteral("power.critical.action"), QStringLiteral("suspend")},
                                  {QStringLiteral("power.critical.countdownSeconds"), 5}};
            if (!adversarial->start()) return false;
        } else {
            auto current = Settings::SettingsSchema::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
            auto old = Settings::SettingsSchema::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
            if (!current || !old) return false;
            service = std::make_unique<Services::SettingsService::ResidentSettingsService>(settingsBus, *current, *old,
                QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
                bus.root.filePath(QStringLiteral("critical-settings.json")));
            if (!service->start().ok()) return false;
        }
        Services::Notifications::FreedesktopServerIdentity identity;
        identity.capabilities = {QStringLiteral("body"), QStringLiteral("actions")};
        if (notificationFault) {
            fault = std::make_unique<CriticalNotificationFault>(notificationBus);
            if (!fault->start()) return false;
        } else {
            host = std::make_unique<Services::NotificationHost::ResidentNotificationHost>(notificationBus, clock, scheduler,
                Services::Notifications::NotificationPolicy{}, identity, nullptr, token);
            if (!host->start().ok()) return false;
        }
        action = std::make_unique<CriticalActionWire>(actionBus);
        if (!action->start()) return false;
        upower = std::make_unique<FakeUpowerService>(upowerBus);
        source(false);
        if (!upower->registerService()) return false;
        transport = std::make_unique<Services::SettingsClient::QtSettingsTransport>(clientBus);
        settings = std::make_unique<Services::SettingsClient::SettingsClient>(*transport,
            QStringList{QStringLiteral("power.critical.action"), QStringLiteral("power.critical.countdownSeconds")});
        if (!settings->start()) return false;
        powerTransport = std::make_unique<Power::QtPowerTransport>(clientBus);
        power = std::make_unique<Power::PowerClient>(powerTransport.get());
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), bus.address);
        env.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), bus.address);
        for (const auto &key : {QStringLiteral("HOME"), QStringLiteral("XDG_CONFIG_HOME"), QStringLiteral("XDG_DATA_HOME"), QStringLiteral("XDG_RUNTIME_DIR")})
            env.insert(key, bus.root.path());
        process.setProcessEnvironment(env);
        QStringList args{QStringLiteral("--upstream=production"), QStringLiteral("--backlight-root=") + bus.root.filePath(QStringLiteral("backlight"))};
        if (enabled) args.append(QStringLiteral("--critical-policy=native-exclusive"));
        process.start(QStringLiteral(QINDAQT_POWER_SERVICE_EXECUTABLE), args);
        if (!process.waitForStarted()) return false;
        qInfo().noquote() << "critical-power-service pid" << process.processId() << "root" << bus.root.path();
        power->start();
        return true;
    }
    void source(bool battery, uint warning = 2) {
        upower->setOnBattery(battery);
        upower->setDevices({{QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0"),
            {{QStringLiteral("Type"), uint(2)}, {QStringLiteral("PowerSupply"), true},
             {QStringLiteral("IsPresent"), true}, {QStringLiteral("State"), uint(2)},
             {QStringLiteral("Percentage"), 2.0}, {QStringLiteral("BatteryLevel"), uint(1)},
             {QStringLiteral("WarningLevel"), warning}}},
            {QStringLiteral("/org/freedesktop/UPower/devices/line_power_AC"),
             {{QStringLiteral("Type"), uint(1)}, {QStringLiteral("Online"), !battery}}}});
        upower->emitServicePropertiesChanged();
    }
    int notifications() const { return int(host->service().snapshot()->notifications.size()); }
    Services::Notifications::NotificationView notification() const { return host->service().snapshot()->notifications.first(); }
    QDBusMessage presentationCall(const QString &method, const QVariantList &args) {
        using Services::NotificationPresentation::WireContract;
        auto msg = QDBusMessage::createMethodCall(notificationBus.baseService(),
            QString::fromLatin1(WireContract::ObjectPath), QString::fromLatin1(WireContract::InterfaceName), method);
        msg.setArguments(args);
        QDBusPendingCallWatcher watcher(presenterBus.asyncCall(msg, 1000));
        QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
        if (!watcher.isFinished()) finished.wait(1000);
        return watcher.reply();
    }
    bool cancelFromPresenter() {
        if (!presenterRegistered) {
            if (presentationCall(QStringLiteral("RegisterPresenter"), {token.toHex()}).type() != QDBusMessage::ReplyMessage) return false;
            presenterRegistered = true;
        }
        const auto n = notification();
        return presentationCall(QStringLiteral("InvokeAction"), {n.id, n.actions.first().key, QString{}}).type() == QDBusMessage::ReplyMessage;
    }
};
}
