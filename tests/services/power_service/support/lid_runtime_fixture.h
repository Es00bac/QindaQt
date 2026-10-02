// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "profile_policy_bus.h"
#include "fake_upower_service.h"
#include "lid_logind_wire.h"
#include "lid_session_wire.h"
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_service/resident_settings_service.h>
#include <QProcessEnvironment>
#include <memory>
namespace QindaQt::Tests {
struct LidRuntime {
    ProfilePolicyBus bus;
    QDBusConnection settingsBus{QStringLiteral("unused-lid-settings")}, logindBus{QStringLiteral("unused-lid-logind")},
        sessionBus{QStringLiteral("unused-lid-session")}, upowerBus{QStringLiteral("unused-lid-upower")},
        clientBus{QStringLiteral("unused-lid-client")}, legacyBus{QStringLiteral("unused-lid-legacy")};
    std::unique_ptr<Services::SettingsService::ResidentSettingsService> service;
    std::unique_ptr<LidLogindWire> logind;
    std::unique_ptr<LidSessionWire> session;
    std::unique_ptr<FakeUpowerService> upower;
    std::unique_ptr<Services::SettingsClient::QtSettingsTransport> settingsTransport;
    std::unique_ptr<Services::SettingsClient::SettingsClient> settings;
    std::unique_ptr<Power::QtPowerTransport> powerTransport;
    std::unique_ptr<Power::PowerClient> power;
    QProcess process;
    ~LidRuntime() { stopResident(); }
    void stopResident() {
        process.terminate();
        if (!process.waitForFinished(2000)) { process.kill(); process.waitForFinished(); }
    }
    bool prepare(bool initialClosed = false, bool legacy = false) {
        if (!bus.start()) return false;
        settingsBus = bus.open(); logindBus = bus.open(); sessionBus = bus.open();
        upowerBus = bus.open(); clientBus = bus.open(); legacyBus = bus.open();
        if (legacy && !legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement"))) return false;
        QString error;
        const auto current = Settings::SettingsSchema::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
        const auto old = Settings::SettingsSchema::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
        if (!current || !old) return false;
        service = std::make_unique<Services::SettingsService::ResidentSettingsService>(settingsBus, *current, *old,
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"), bus.root.filePath(QStringLiteral("lid-settings.json")));
        if (!service->start().ok()) return false;
        logind = std::make_unique<LidLogindWire>(logindBus);
        logind->domain.setSessionTruth(initialClosed, false, false);
        if (!logind->start()) return false;
        session = std::make_unique<LidSessionWire>(sessionBus);
        if (!session->start()) return false;
        upower = std::make_unique<FakeUpowerService>(upowerBus);
        source(false);
        if (!upower->registerService()) return false;
        settingsTransport = std::make_unique<Services::SettingsClient::QtSettingsTransport>(clientBus);
        QStringList keys;
        for (const auto &s : {QStringLiteral("ac"), QStringLiteral("battery"), QStringLiteral("lowBattery")})
            keys << QStringLiteral("power.lid.") + s + QStringLiteral(".action") << QStringLiteral("power.lid.") + s + QStringLiteral(".dockedAction");
        settings = std::make_unique<Services::SettingsClient::SettingsClient>(*settingsTransport, keys);
        if (!settings->start()) return false;
        powerTransport = std::make_unique<Power::QtPowerTransport>(clientBus);
        power = std::make_unique<Power::PowerClient>(powerTransport.get());
        return true;
    }
    bool start(bool enabled = true, bool installed = false) {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), bus.address);
        env.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), bus.address);
        for (const auto &key : {QStringLiteral("HOME"), QStringLiteral("XDG_CONFIG_HOME"), QStringLiteral("XDG_DATA_HOME"), QStringLiteral("XDG_RUNTIME_DIR")}) env.insert(key, bus.root.path());
        for (const auto &key : {QStringLiteral("DISPLAY"), QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("DBUS_STARTER_ADDRESS"), QStringLiteral("DBUS_STARTER_BUS_TYPE")}) env.remove(key);
        process.setProcessEnvironment(env);
        QStringList args{QStringLiteral("--upstream=production"), QStringLiteral("--backlight-root=") + bus.root.filePath(QStringLiteral("backlight"))};
        if (enabled) args << QStringLiteral("--lid-policy=native-exclusive");
        process.start(QString::fromUtf8(installed ? QINDAQT_POWER_SERVICE_EXECUTABLE : QINDAQT_LID_PRIVATE_RESIDENT), args);
        if (!process.waitForStarted()) return false;
        qInfo().noquote() << "lid-power-service pid" << process.processId() << "root" << bus.root.path();
        power->start();
        return true;
    }
    void source(bool battery, uint warning = 2) {
        upower->setOnBattery(battery);
        upower->setDevices({{QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0"),
            {{QStringLiteral("Type"), uint(2)}, {QStringLiteral("PowerSupply"), true}, {QStringLiteral("IsPresent"), true},
             {QStringLiteral("State"), uint(2)}, {QStringLiteral("Percentage"), 50.0}, {QStringLiteral("WarningLevel"), warning}}},
            {QStringLiteral("/org/freedesktop/UPower/devices/line_power_AC"), {{QStringLiteral("Type"), uint(1)}, {QStringLiteral("Online"), !battery}}}});
        upower->emitServicePropertiesChanged();
    }
    QStringList actions() const {
        auto result = session->actions;
        for (const auto &action : logind->domain.actionCalls) result.append(action.method);
        return result;
    }
    bool preference(const QString &key, const QVariant &value) { QString error; return settings->setUserValue(key, value, &error); }
};
}
