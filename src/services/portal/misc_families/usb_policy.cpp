// SPDX-License-Identifier: LGPL-3.0-only
// Adapted from xdg-desktop-portal-kde v6.6.6 usb.cpp, David Redondo (2025).
#include "misc_policy.h"
#include <QJsonArray>
#include <QDBusMetaType>
#include <QSet>
#include <QRegularExpression>
namespace QindaQt::Services::Portal {
std::optional<QJsonArray> usbDevicesFrame(const UsbDevices &devices) {
    if (devices.isEmpty() || devices.size() > 64) return {};
    QSet<QString> ids; QJsonArray result;
    for (const auto &[id, information, options] : devices) {
        if (id.isEmpty() || !boundedText(id, 256) || ids.contains(id) || information.size() > 32 || options.size() > 32) return {};
        ids.insert(id);
        if (options.contains("writable") && options.value("writable").metaType() != QMetaType::fromType<bool>()) return {};
        QVariantMap properties;
        const auto value = information.value("properties");
        if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
            const auto argument = value.value<QDBusArgument>();
            if (argument.currentSignature() != QStringLiteral("a{sv}")) return {};
            properties = qdbus_cast<QVariantMap>(argument);
        } else if (value.metaType() == QMetaType::fromType<QVariantMap>()) properties = value.toMap();
        else if (value.isValid()) return {};
        auto identity = [&properties](const char *database, const char *encoded, const char *numeric) {
            QString text = properties.value(database, properties.value(encoded, properties.value(numeric))).toString();
            if (!boundedText(text, 1024)) return QString{};
            const QRegularExpression hex(QStringLiteral(R"(\\x([\da-fA-F]{2}))"));
            auto matches = hex.globalMatch(text); QList<QRegularExpressionMatch> found;
            while (matches.hasNext()) found.append(matches.next());
            for (auto match = found.crbegin(); match != found.crend(); ++match) text.replace(match->capturedStart(), match->capturedLength(), QChar(match->captured(1).toUShort(nullptr, 16)));
            return boundedText(text, 1024) ? text : QString{};
        };
        const QString model = identity("ID_MODEL_FROM_DATABASE", "ID_MODEL_ENC", "ID_MODEL_ID");
        const QString vendor = identity("ID_VENDOR_FROM_DATABASE", "ID_VENDOR_ENC", "ID_VENDOR_ID");
        const QString label = (vendor + QLatin1Char(' ') + model).trimmed() + QStringLiteral(" [%1]").arg(id);
        result.append(QJsonObject{{"id", id}, {"label", label}, {"writable", options.value("writable", false).toBool()}});
    }
    return result;
}
std::optional<UsbSelections> usbResults(const UsbDevices &offered, const QJsonObject &output) {
    if (output.size() != 1 || !output.value("devices").isArray()) return {};
    UsbSelections selected; QSet<QString> seen;
    for (const auto &value : output.value("devices").toArray()) {
        if (!value.isString() || seen.contains(value.toString())) return {};
        const auto id = value.toString(); seen.insert(id); bool found = false;
        for (const auto &[offeredId, information, options] : offered) {
            Q_UNUSED(information)
            if (id == offeredId) { selected.append({id, QVariantMap{{"writable", options.value("writable", false).toBool()}}}); found = true; break; }
        }
        if (!found) return {};
    }
    return selected;
}
}
