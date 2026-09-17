// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_protocol/wiz_messages.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace QindaQt::Wiz
{
namespace
{

[[nodiscard]] QByteArray encodeCall(const QString &method, const QJsonObject &params)
{
    QJsonObject root;
    root.insert(QStringLiteral("method"), method);
    root.insert(QStringLiteral("params"), params);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace

Method methodFromString(const QString &value)
{
    if (value == QLatin1String("getPilot")) {
        return Method::GetPilot;
    }
    if (value == QLatin1String("setPilot")) {
        return Method::SetPilot;
    }
    if (value == QLatin1String("syncPilot")) {
        return Method::SyncPilot;
    }
    if (value == QLatin1String("registration")) {
        return Method::Registration;
    }
    if (value == QLatin1String("getSystemConfig")) {
        return Method::GetSystemConfig;
    }
    if (value == QLatin1String("getModelConfig")) {
        return Method::GetModelConfig;
    }
    return Method::Unknown;
}

QByteArray encodeGetPilot()
{
    return encodeCall(QStringLiteral("getPilot"), QJsonObject());
}

QByteArray encodeGetSystemConfig()
{
    return encodeCall(QStringLiteral("getSystemConfig"), QJsonObject());
}

QByteArray encodeGetModelConfig()
{
    return encodeCall(QStringLiteral("getModelConfig"), QJsonObject());
}

QByteArray encodeRegistration(const QString &listenerAddress,
                              const QString &listenerMac, const bool subscribe)
{
    // AGENT-CONTRACT: the firmware keys its notification subscription on the
    // (phoneIp, phoneMac) pair it is told here and pushes syncPilot to UDP
    // 38900 at that address. A caller that cannot listen on that port must
    // pass subscribe=false, which is also the vendor's discovery ping.
    QJsonObject params;
    params.insert(QStringLiteral("phoneIp"), listenerAddress);
    params.insert(QStringLiteral("phoneMac"), listenerMac);
    params.insert(QStringLiteral("register"), subscribe);
    return encodeCall(QStringLiteral("registration"), params);
}

QByteArray encodeSetPilot(const StateRequest &request)
{
    if (request.isEmpty()) {
        return {};
    }
    QJsonObject params;
    if (request.setPower) {
        params.insert(QStringLiteral("state"), request.on);
    }
    if (request.setScene) {
        params.insert(QStringLiteral("sceneId"), static_cast<int>(request.sceneId));
    }
    if (request.setTemperature) {
        params.insert(QStringLiteral("temp"), static_cast<int>(request.temperatureKelvin));
    }
    if (request.setColor) {
        params.insert(QStringLiteral("r"), static_cast<int>(request.red));
        params.insert(QStringLiteral("g"), static_cast<int>(request.green));
        params.insert(QStringLiteral("b"), static_cast<int>(request.blue));
        params.insert(QStringLiteral("c"), static_cast<int>(request.coolWhite));
        params.insert(QStringLiteral("w"), static_cast<int>(request.warmWhite));
    }
    if (request.setDimming) {
        params.insert(QStringLiteral("dimming"), static_cast<int>(request.dimmingPercent));
    }
    if (request.setSpeed) {
        params.insert(QStringLiteral("speed"), static_cast<int>(request.speedPercent));
    }
    return encodeCall(QStringLiteral("setPilot"), params);
}

} // namespace QindaQt::Wiz
