// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_bluez.h"

#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>

#include <utility>

namespace QindaQt::Tests
{

void FakeBluez::registerAgent(const QDBusMessage &request)
{
    ++registerAgentCalls;
    const QVariantList arguments = request.arguments();
    const QString path = arguments.value(0).value<QDBusObjectPath>().path();
    const QString capability = arguments.value(1).toString();
    if (path.isEmpty() || capability != QLatin1String("KeyboardDisplay")) {
        sendError(request, QStringLiteral("org.bluez.Error.InvalidArguments"),
                  QStringLiteral("Invalid agent"));
        return;
    }
    m_agentOwner = request.service();
    m_agentPath = path;
    registeredCapability = capability;
    sendReply(request);
}

void FakeBluez::unregisterAgent(const QDBusMessage &request)
{
    ++unregisterAgentCalls;
    const QString path = request.arguments().value(0).value<QDBusObjectPath>().path();
    if (request.service() != m_agentOwner || path != m_agentPath) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Agent is not registered"));
        return;
    }
    m_agentOwner.clear();
    m_agentPath.clear();
    registeredCapability.clear();
    sendReply(request);
}

void FakeBluez::devicePair(const QString &path, const QDBusMessage &request)
{
    ++pairCalls;
    DeviceEntity *entity = device(path);
    if (entity == nullptr) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Device does not exist"));
        return;
    }
    if (entity->paired) {
        sendError(request, QStringLiteral("org.bluez.Error.AlreadyExists"),
                  QStringLiteral("Already paired"));
        return;
    }
    if (m_agentOwner.isEmpty() || m_agentPath.isEmpty()) {
        sendError(request, QStringLiteral("org.bluez.Error.Failed"),
                  QStringLiteral("No agent"));
        return;
    }
    entity->deferredPairRequests.append(request);
    dispatchAgentRequest(path);
}

void FakeBluez::dispatchAgentRequest(const QString &devicePath)
{
    DeviceEntity *entity = device(devicePath);
    if (entity == nullptr) return;
    QString member;
    QList<QVariant> arguments{QVariant::fromValue(QDBusObjectPath(devicePath))};
    switch (entity->pairingMode) {
    case PairingMode::ConfirmPasskey:
        member = QStringLiteral("RequestConfirmation");
        arguments.append(entity->pairingPasskey);
        break;
    case PairingMode::RequestPasskey:
        member = QStringLiteral("RequestPasskey");
        break;
    case PairingMode::RequestPin:
        member = QStringLiteral("RequestPinCode");
        break;
    case PairingMode::DisplayPasskey:
        member = QStringLiteral("DisplayPasskey");
        arguments.append(QVariant::fromValue(entity->pairingPasskey));
        arguments.append(QVariant::fromValue(entity->entered));
        break;
    case PairingMode::DisplayPin:
        member = QStringLiteral("DisplayPinCode");
        arguments.append(entity->pairingPin);
        break;
    case PairingMode::AuthorizeService:
        member = QStringLiteral("AuthorizeService");
        arguments.append(entity->serviceUuid);
        break;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        m_agentOwner, m_agentPath, QStringLiteral("org.bluez.Agent1"), member);
    call.setArguments(arguments);
    const PairingMode mode = entity->pairingMode;
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, devicePath, mode](QDBusPendingCallWatcher *) {
                const QDBusMessage response = watcher->reply();
                watcher->deleteLater();
                DeviceEntity *current = device(devicePath);
                if (current == nullptr || current->deferredPairRequests.isEmpty()) return;
                if (response.type() != QDBusMessage::ReplyMessage) {
                    const QList<QDBusMessage> requests =
                        std::exchange(current->deferredPairRequests, {});
                    for (const QDBusMessage &request : requests) {
                        sendError(request,
                                  QStringLiteral("org.bluez.Error.AuthenticationRejected"),
                                  QStringLiteral("Agent rejected pairing"));
                    }
                    return;
                }
                if (mode == PairingMode::DisplayPasskey
                    || mode == PairingMode::DisplayPin) return;
                if (mode == PairingMode::RequestPasskey) {
                    lastPasskeyReply = response.arguments().value(0).toUInt();
                } else if (mode == PairingMode::RequestPin) {
                    lastPinReply = response.arguments().value(0).toString();
                }
                current->paired = true;
                emitDeviceProperties(devicePath, {{QStringLiteral("Paired"), true}});
                const QList<QDBusMessage> requests =
                    std::exchange(current->deferredPairRequests, {});
                for (const QDBusMessage &request : requests) sendReply(request);
            });
}

void FakeBluez::deviceCancelPairing(const QString &path,
                                    const QDBusMessage &request)
{
    ++cancelPairingCalls;
    DeviceEntity *entity = device(path);
    if (entity == nullptr || entity->deferredPairRequests.isEmpty()) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("No pairing"));
        return;
    }
    if (!m_agentOwner.isEmpty()) {
        QDBusMessage cancel = QDBusMessage::createMethodCall(
            m_agentOwner, m_agentPath, QStringLiteral("org.bluez.Agent1"),
            QStringLiteral("Cancel"));
        m_connection.asyncCall(cancel);
    }
    const QList<QDBusMessage> requests =
        std::exchange(entity->deferredPairRequests, {});
    for (const QDBusMessage &pairRequest : requests) {
        sendError(pairRequest, QStringLiteral("org.bluez.Error.AuthenticationCanceled"),
                  QStringLiteral("Pairing canceled"));
    }
    sendReply(request);
}

void FakeBluez::adapterRemoveDevice(const QString &path,
                                    const QDBusMessage &request)
{
    ++removeDeviceCalls;
    const QString devicePath = request.arguments().value(0).value<QDBusObjectPath>().path();
    DeviceEntity *entity = device(devicePath);
    if (entity == nullptr || entity->adapterPath != path) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Device does not exist"));
        return;
    }
    removeDeviceObject(devicePath);
    sendReply(request);
}

void FakeBluez::propertySet(const QString &path, const QString &interfaceName,
                            const QString &name, const QVariant &value,
                            const QDBusMessage &request)
{
    if (interfaceName == QLatin1String("org.bluez.Adapter1")
        && name == QLatin1String("Powered") && value.canConvert<bool>()) {
        setAdapterPowered(path, value.toBool());
        sendReply(request);
        return;
    }
    if (interfaceName == QLatin1String("org.bluez.Device1")
        && name == QLatin1String("Trusted") && value.canConvert<bool>()) {
        DeviceEntity *entity = device(path);
        if (entity == nullptr) {
            sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                      QStringLiteral("Device does not exist"));
            return;
        }
        entity->trusted = value.toBool();
        emitDeviceProperties(path, {{QStringLiteral("Trusted"), entity->trusted}});
        sendReply(request);
        return;
    }
    sendError(request, QStringLiteral("org.bluez.Error.Failed"),
              QStringLiteral("Unknown property"));
}

} // namespace QindaQt::Tests
