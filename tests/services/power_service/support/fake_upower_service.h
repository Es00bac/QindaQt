// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>

namespace QindaQt::Tests {

// A fake org.freedesktop.UPower service implemented as a QDBusVirtualObject so
// every property has an exact, test-controlled D-Bus type and any value (or
// wrong type, or absence) can be delivered. Replies are wire-faithful: the
// service object answers Properties.GetAll for org.freedesktop.UPower, each
// device path answers for org.freedesktop.UPower.Device, and signals are sent
// exactly as the real daemon sends them.
class FakeUpowerService final : public QDBusVirtualObject
{
public:
    struct DeviceSpec {
        QString path;
        QVariantMap properties;
    };

    explicit FakeUpowerService(const QDBusConnection &connection,
                               QObject *parent = nullptr);
    ~FakeUpowerService() override;

    bool registerService();
    void unregisterService();

    void setDevices(const QList<DeviceSpec> &devices);
    void setOnBattery(bool onBattery);
    void emitDeviceAdded(const QString &path);
    void emitDeviceRemoved(const QString &path);
    void emitDevicePropertiesChanged(const QString &path);
    void emitServicePropertiesChanged();

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override;

private:
    void sendError(const QDBusMessage &message, const QString &name,
                   const QString &text);
    [[nodiscard]] QDBusMessage serviceProperties() const;

    QDBusConnection m_connection;
    QList<DeviceSpec> m_devices;
    bool m_onBattery = false;
};

} // namespace QindaQt::Tests
