// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/firewall_protocol/firewall_codec.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

namespace QindaQt::Firewall {
namespace {

QString stringValue(const QJsonObject &object, const QLatin1StringView key)
{
    return object.value(key).toString();
}

bool boolValue(const QJsonObject &object, const QLatin1StringView key)
{
    return object.value(key).toBool(false);
}

QStringList stringListValue(const QJsonObject &object, const QLatin1StringView key)
{
    QStringList result;
    const QJsonValue value = object.value(key);
    if (!value.isArray()) {
        return result;
    }
    const QJsonArray array = value.toArray();
    result.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (entry.isString()) {
            result.append(entry.toString());
        }
    }
    return result;
}

bool decodeAlertObject(const QJsonObject &object, Alert *out)
{
    out->id = stringValue(object, QLatin1StringView("id"));
    if (out->id.isEmpty()) {
        // AGENT-GUARD: every documented alert carries a stable, non-empty
        // id (Dismiss/ConfirmMine/OpenWindow key on it); an object without
        // one is not a decodable alert, even if every other field parses.
        return false;
    }
    out->kind = stringValue(object, QLatin1StringView("kind"));
    out->severity = alertSeverityFromString(
        stringValue(object, QLatin1StringView("severity")));
    out->severityLabel = stringValue(object, QLatin1StringView("severityLabel"));
    out->title = stringValue(object, QLatin1StringView("title"));
    out->summary = stringValue(object, QLatin1StringView("summary"));
    out->meaning = stringValue(object, QLatin1StringView("meaning"));
    out->advice = stringValue(object, QLatin1StringView("advice"));
    out->canBlock = boolValue(object, QLatin1StringView("canBlock"));
    out->canConfirm = boolValue(object, QLatin1StringView("canConfirm"));
    out->address = stringValue(object, QLatin1StringView("address"));
    out->place = stringValue(object, QLatin1StringView("place"));
    out->user = stringValue(object, QLatin1StringView("user"));
    out->count = static_cast<quint64>(
        object.value(QLatin1StringView("count")).toInteger(0));
    out->firstText = stringValue(object, QLatin1StringView("firstText"));
    out->lastText = stringValue(object, QLatin1StringView("lastText"));
    out->ongoing = boolValue(object, QLatin1StringView("ongoing"));
    out->remote = boolValue(object, QLatin1StringView("remote"));
    out->evidence = stringListValue(object, QLatin1StringView("evidence"));
    out->mac = stringValue(object, QLatin1StringView("mac"));
    return true;
}

} // namespace

LogWatcherState logWatcherStateFromString(const QString &value)
{
    if (value == QLatin1StringView("off")) return LogWatcherState::Off;
    if (value == QLatin1StringView("starting")) return LogWatcherState::Starting;
    if (value == QLatin1StringView("live")) return LogWatcherState::Live;
    if (value == QLatin1StringView("denied")) return LogWatcherState::Denied;
    if (value == QLatin1StringView("retrying")) return LogWatcherState::Retrying;
    return LogWatcherState::Unknown;
}

QString toString(const LogWatcherState value)
{
    switch (value) {
    case LogWatcherState::Off: return QStringLiteral("off");
    case LogWatcherState::Starting: return QStringLiteral("starting");
    case LogWatcherState::Live: return QStringLiteral("live");
    case LogWatcherState::Denied: return QStringLiteral("denied");
    case LogWatcherState::Retrying: return QStringLiteral("retrying");
    case LogWatcherState::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

HeadlineLevel headlineLevelFromString(const QString &value)
{
    if (value == QLatin1StringView("ok")) return HeadlineLevel::Ok;
    if (value == QLatin1StringView("notice")) return HeadlineLevel::Notice;
    if (value == QLatin1StringView("warning")) return HeadlineLevel::Warning;
    if (value == QLatin1StringView("danger")) return HeadlineLevel::Danger;
    return HeadlineLevel::Unknown;
}

QString toString(const HeadlineLevel value)
{
    switch (value) {
    case HeadlineLevel::Ok: return QStringLiteral("ok");
    case HeadlineLevel::Notice: return QStringLiteral("notice");
    case HeadlineLevel::Warning: return QStringLiteral("warning");
    case HeadlineLevel::Danger: return QStringLiteral("danger");
    case HeadlineLevel::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

AlertSeverity alertSeverityFromString(const QString &value)
{
    if (value == QLatin1StringView("notice")) return AlertSeverity::Notice;
    if (value == QLatin1StringView("warning")) return AlertSeverity::Warning;
    if (value == QLatin1StringView("danger")) return AlertSeverity::Danger;
    return AlertSeverity::Info;
}

QString toString(const AlertSeverity value)
{
    switch (value) {
    case AlertSeverity::Info: return QStringLiteral("info");
    case AlertSeverity::Notice: return QStringLiteral("notice");
    case AlertSeverity::Warning: return QStringLiteral("warning");
    case AlertSeverity::Danger: return QStringLiteral("danger");
    }
    return QStringLiteral("info");
}

bool decodeHeadline(const QByteArray &json, Headline *out)
{
    if (out == nullptr) {
        return false;
    }
    *out = Headline{};
    if (json.isEmpty()) {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject object = document.object();
    out->level = headlineLevelFromString(stringValue(object, QLatin1StringView("level")));
    out->title = stringValue(object, QLatin1StringView("title"));
    out->text = stringValue(object, QLatin1StringView("text"));
    return true;
}

bool decodeAlert(const QByteArray &json, Alert *out)
{
    if (out == nullptr) {
        return false;
    }
    *out = Alert{};
    // AGENT-NOTE: "" is the documented no-alert value for TopAlert/Alert(id)
    // (QindaFirewall docs/monitor.md); it is not malformed JSON, but it is
    // still "no alert decoded" to this function's caller.
    if (json.isEmpty()) {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    return decodeAlertObject(document.object(), out);
}

QList<Alert> decodeAlertList(const QByteArray &json)
{
    QList<Alert> result;
    if (json.isEmpty()) {
        return result;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return result;
    }
    const QJsonArray array = document.array();
    result.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isObject()) {
            continue;
        }
        Alert alert;
        if (decodeAlertObject(entry.toObject(), &alert)) {
            result.append(std::move(alert));
        }
    }
    return result;
}

} // namespace QindaQt::Firewall
