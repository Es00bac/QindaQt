// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtDBus/QDBusVirtualObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>
namespace QindaQt::Tests {
// One wire-faithful, adversarial Settings1 source for revision regression.
// Production client decoding and actual resident policy remain untouched.
class ProfileSettingsSource final : public QDBusVirtualObject {
public:
    explicit ProfileSettingsSource(QDBusConnection connection) : m_bus(std::move(connection)) {}
    ~ProfileSettingsSource() override {
        m_bus.unregisterService(QStringLiteral("org.qindaqt.Settings1"));
        m_bus.unregisterObject(QStringLiteral("/org/qindaqt/Settings1"));
    }
    bool start() {
        return m_bus.registerVirtualObject(QStringLiteral("/org/qindaqt/Settings1"), this)
            && m_bus.registerService(QStringLiteral("org.qindaqt.Settings1"));
    }
    void invalidate(quint64 advertised) {
        auto signal = QDBusMessage::createSignal(QStringLiteral("/org/qindaqt/Settings1"),
            QStringLiteral("org.qindaqt.Settings1"), QStringLiteral("SettingsChanged"));
        signal.setArguments({epoch, QVariant::fromValue(advertised), values.keys()});
        m_bus.send(signal);
    }
    QString introspect(const QString &) const override { return QStringLiteral("<node/>"); }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        using Services::SettingsProtocol::WireContract;
        using Services::SettingsProtocol::SettingsWireStatus;
        if (message.interface() != QStringLiteral("org.qindaqt.Settings1")
            || message.member() != QStringLiteral("GetSnapshot")) return false;
        QVariantMap layers;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            layers.insert(it.key(), QStringLiteral("user-overrides"));
        const QVariantMap wire{
            {QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
        auto reply = message.createReply(); reply.setArguments({wire}); m_bus.send(reply);
        return true;
    }
    QString epoch = QStringLiteral("adversarial-profile-settings");
    quint64 revision = 10;
    QVariantMap values{{QStringLiteral("power.profile.ac"), QStringLiteral("performance")},
                       {QStringLiteral("power.profile.battery"), QStringLiteral("power-saver")},
                       {QStringLiteral("power.profile.lowBattery"), QStringLiteral("power-saver")}};
private:
    QDBusConnection m_bus;
};
}
