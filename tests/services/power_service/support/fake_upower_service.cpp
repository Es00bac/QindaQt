// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_upower_service.h"

#include <QtDBus/QDBusArgument>


#include <QtCore/QVariant>

namespace QindaQt::Tests {
namespace {

constexpr char kServiceName[] = "org.freedesktop.UPower";
constexpr char kServicePath[] = "/org/freedesktop/UPower";
constexpr char kServiceInterface[] = "org.freedesktop.UPower";
constexpr char kDeviceInterface[] = "org.freedesktop.UPower.Device";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

QVariantMap servicePropertyMap(const bool onBattery)
{
    QVariantMap properties;
    properties.insert(QStringLiteral("DaemonVersion"), QVariant(QStringLiteral("fake-1.0")));
    properties.insert(QStringLiteral("OnBattery"), QVariant(onBattery));
    return properties;
}

} // namespace

FakeUpowerService::FakeUpowerService(const QDBusConnection &connection,
                                     QObject *parent)
    : QDBusVirtualObject(parent)
    , m_connection(connection)
{
}

FakeUpowerService::~FakeUpowerService()
{
    unregisterService();
}

bool FakeUpowerService::registerService()
{
    if (!m_connection.registerVirtualObject(QString::fromLatin1(kServicePath), this,
                                            QDBusConnection::SubPath)) {
        return false;
    }
    if (!m_connection.registerService(QString::fromLatin1(kServiceName))) {
        m_connection.unregisterObject(QString::fromLatin1(kServicePath));
        return false;
    }
    return true;
}

void FakeUpowerService::unregisterService()
{
    m_connection.unregisterObject(QString::fromLatin1(kServicePath));
    m_connection.unregisterService(QString::fromLatin1(kServiceName));
}

void FakeUpowerService::setDevices(const QList<DeviceSpec> &devices)
{
    m_devices = devices;
}

void FakeUpowerService::setOnBattery(const bool onBattery)
{
    m_onBattery = onBattery;
}

void FakeUpowerService::emitDeviceAdded(const QString &path)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kServicePath), QString::fromLatin1(kServiceInterface),
        QStringLiteral("DeviceAdded"));
    signal.setArguments(
        {QVariant::fromValue(QDBusObjectPath(path))});
    m_connection.send(signal);
}

void FakeUpowerService::emitDeviceRemoved(const QString &path)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kServicePath), QString::fromLatin1(kServiceInterface),
        QStringLiteral("DeviceRemoved"));
    signal.setArguments(
        {QVariant::fromValue(QDBusObjectPath(path))});
    m_connection.send(signal);
}

void FakeUpowerService::emitDevicePropertiesChanged(const QString &path)
{
    for (const DeviceSpec &device : m_devices) {
        if (device.path != path) {
            continue;
        }
        QDBusMessage signal = QDBusMessage::createSignal(
            path, QString::fromLatin1(kPropertiesInterface),
            QStringLiteral("PropertiesChanged"));
        signal.setArguments(
            {QString::fromLatin1(kDeviceInterface), QVariant(device.properties),
             QStringList{}});
        m_connection.send(signal);
        return;
    }
}

void FakeUpowerService::emitServicePropertiesChanged()
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kServicePath), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"));
    signal.setArguments({QString::fromLatin1(kServiceInterface),
                         QVariant(servicePropertyMap(m_onBattery)),
                         QStringList{}});
    m_connection.send(signal);
}

QString FakeUpowerService::introspect(const QString &path) const
{
    Q_UNUSED(path)
    return QStringLiteral("<node/>");
}

void FakeUpowerService::sendError(const QDBusMessage &message, const QString &name,
                                  const QString &text)
{
    QDBusMessage error = message.createErrorReply(name, text);
    m_connection.send(error);
}

bool FakeUpowerService::handleMessage(const QDBusMessage &message,
                                      const QDBusConnection &connection)
{
    Q_UNUSED(connection)
    if (message.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    const QString member = message.member();

    if (message.interface() == QString::fromLatin1(kServiceInterface)) {
        if (member == QStringLiteral("EnumerateDevices")) {
            QList<QDBusObjectPath> paths;
            for (const DeviceSpec &device : m_devices) {
                paths.push_back(QDBusObjectPath(device.path));
            }
            QDBusMessage reply = message.createReply();
            reply.setArguments({QVariant::fromValue(paths)});
            m_connection.send(reply);
            return true;
        }
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                  QStringLiteral("Unknown member ") + member);
        return true;
    }
    if (message.interface() == QString::fromLatin1(kPropertiesInterface)) {
        if (message.arguments().size() != 1) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                      QStringLiteral("Bad Properties call"));
            return true;
        }
        const QString interfaceName = message.arguments().constFirst().toString();
        if (interfaceName == QString::fromLatin1(kServiceInterface)
            && message.path() == QString::fromLatin1(kServicePath)) {
            QDBusMessage reply = message.createReply();
            reply.setArguments(
                {QVariant(servicePropertyMap(m_onBattery))});
            m_connection.send(reply);
            return true;
        }
        if (interfaceName == QString::fromLatin1(kDeviceInterface)) {
            for (const DeviceSpec &device : m_devices) {
                if (device.path == message.path()) {
                    QDBusMessage reply = message.createReply();
                    reply.setArguments({QVariant(device.properties)});
                    m_connection.send(reply);
                    return true;
                }
            }
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"),
                      QStringLiteral("No such device"));
            return true;
        }
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface"),
                  QStringLiteral("Unknown interface ") + interfaceName);
        return true;
    }
    sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
              QStringLiteral("Unsupported interface"));
    return true;
}

} // namespace QindaQt::Tests
