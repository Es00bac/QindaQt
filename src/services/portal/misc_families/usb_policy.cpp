// SPDX-License-Identifier: LGPL-3.0-only
// Adapted from xdg-desktop-portal-kde v6.6.6 usb.cpp, David Redondo (2025).
#include "misc_policy.h"
#include <QJsonArray>
#include <QDBusMetaType>
#include <QSet>
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
        QString label = properties.value("ID_MODEL_FROM_DATABASE", properties.value("ID_MODEL", id)).toString();
        if (!boundedText(label, 1024)) return {};
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
