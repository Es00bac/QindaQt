// SPDX-License-Identifier: LGPL-3.0-or-later
#include "print_policy.h"
#include <QJsonArray>
#include <cmath>
namespace QindaQt::Services::Portal {
bool validPrintMaps(const QVariantMap &settings, const QVariantMap &pages) {
    if (settings.size() > 64 || pages.size() > 32) return false;
    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        if (!boundedText(it.key(), 128) || it.value().metaType() != QMetaType::fromType<QString>() || !boundedText(it.value().toString(), 4096)) return false;
    }
    for (auto it = pages.cbegin(); it != pages.cend(); ++it) {
        if (!boundedText(it.key(), 128)) return false;
        if (QStringList{"Width", "Height", "MarginTop", "MarginBottom", "MarginLeft", "MarginRight"}.contains(it.key())) {
            if (it.value().metaType() != QMetaType::fromType<double>() || !std::isfinite(it.value().toDouble()) || it.value().toDouble() < 0 || it.value().toDouble() > 10000) return false;
        } else if (it.value().metaType() != QMetaType::fromType<QString>() || !boundedText(it.value().toString(), 1024)) return false;
    }
    for (const auto *key : {"n-copies", "resolution"}) if (settings.contains(key)) {
        bool ok = false; const int n = settings.value(key).toString().toInt(&ok);
        if (!ok || n < 1 || n > (QByteArray(key) == "n-copies" ? 999 : 9600)) return false;
    }
    return true;
}
bool validPrintConfiguration(const QJsonObject &output) {
    if (output.size() != 5 || !output.value("settings").isObject() || !output.value("page-setup").isObject()
        || !output.value("printer").isString() || !output.value("output").isString() || !output.value("cups").isArray()) return false;
    if (!validPrintMaps(output.value("settings").toObject().toVariantMap(), output.value("page-setup").toObject().toVariantMap())) return false;
    if (!boundedText(output.value("printer").toString(), 1024) || !boundedText(output.value("output").toString(), 4096)) return false;
    const auto destination = output.value("output").toString();
    if (!destination.isEmpty() && !destination.startsWith(QLatin1Char('/'))) return false;
    const auto cups = output.value("cups").toArray(); if (cups.size() > 128 || cups.size() % 2) return false;
    for (const auto &value : cups) if (!value.isString() || !boundedText(value.toString(), 1024)) return false;
    return !output.value("printer").toString().isEmpty() || output.value("output").toString().startsWith(QLatin1Char('/'));
}
}
