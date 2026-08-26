// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"

#include "qindaqt/applets/manifest_types.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace QindaQt::AppletHost {
namespace {

CapabilityPolicyLoadResult failure(const QString &origin, const QString &message)
{
    return {.ok = false, .policy = {}, .error = origin + QStringLiteral(": ") + message};
}

} // namespace

CapabilityPolicyLoadResult CapabilityPolicyLoader::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path, file.errorString());
    }
    return fromJson(file.readAll(), path);
}

CapabilityPolicyLoadResult CapabilityPolicyLoader::fromJson(const QByteArray &json,
                                                             const QString &origin)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(origin, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    }

    const QJsonObject root = document.object();
    CapabilityPolicy policy;
    policy.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);

    const QJsonValue defaultsValue = root.value(QStringLiteral("defaults"));
    if (!defaultsValue.isObject()) {
        return failure(origin, QStringLiteral("defaults must be an object"));
    }
    const QJsonObject defaults = defaultsValue.toObject();
    const auto builtinDefault = capabilityDispositionFromString(
        defaults.value(QStringLiteral("auditedBuiltin")).toString());
    const auto thirdPartyDefault = capabilityDispositionFromString(
        defaults.value(QStringLiteral("thirdParty")).toString());
    if (!builtinDefault.has_value() || !thirdPartyDefault.has_value()) {
        return failure(origin, QStringLiteral("defaults must explicitly be 'grant' or 'deny'"));
    }
    policy.auditedBuiltinDefault = *builtinDefault;
    policy.thirdPartyDefault = *thirdPartyDefault;

    const QJsonValue rulesValue = root.value(QStringLiteral("rules"));
    if (!rulesValue.isArray()) {
        return failure(origin, QStringLiteral("rules must be an array"));
    }
    for (const QJsonValue &value : rulesValue.toArray()) {
        if (!value.isObject()) {
            return failure(origin, QStringLiteral("every rule must be an object"));
        }
        const QJsonObject object = value.toObject();
        const auto trust = packageTrustFromString(object.value(QStringLiteral("trust")).toString());
        const auto capability = QindaQt::Applets::capabilityFromString(
            object.value(QStringLiteral("capability")).toString());
        const auto disposition = capabilityDispositionFromString(
            object.value(QStringLiteral("decision")).toString());
        if (!trust.has_value() || !capability.has_value() || !disposition.has_value()) {
            return failure(origin,
                           QStringLiteral("rule contains an unknown trust, capability, or decision"));
        }

        QString packageId = object.value(QStringLiteral("packageId")).toString();
        if (packageId == QLatin1String("*")) {
            packageId.clear();
        }
        policy.rules.append({.trust = *trust,
                             .packageId = packageId,
                             .capability = *capability,
                             .disposition = *disposition,
                             .reason = object.value(QStringLiteral("reason")).toString()});
    }

    const PolicyValidation validation = policy.validate();
    if (!validation.isValid()) {
        return failure(origin, validation.summary());
    }
    return {.ok = true, .policy = policy, .error = {}};
}

} // namespace QindaQt::AppletHost
