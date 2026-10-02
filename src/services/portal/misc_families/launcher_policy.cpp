// SPDX-License-Identifier: LGPL-3.0-only
// Adapted from xdg-desktop-portal-kde v6.6.6 dynamiclauncher.cpp,
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
#include "misc_policy.h"
#include <QDBusMetaType>
namespace QindaQt::Services::Portal {
QDBusArgument &operator<<(QDBusArgument &a, const LauncherIcon &icon) { a.beginStructure(); a << icon.kind << icon.data; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, LauncherIcon &icon) { a.beginStructure(); a >> icon.kind >> icon.data; a.endStructure(); return a; }
std::optional<LauncherIcon> launcherIcon(const QDBusVariant &variant) {
    LauncherIcon icon;
    const auto value = variant.variant();
    if (value.metaType() == QMetaType::fromType<LauncherIcon>()) icon = value.value<LauncherIcon>();
    else if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const auto argument = value.value<QDBusArgument>(); if (argument.currentSignature() != QStringLiteral("(sv)")) return {};
        icon = qdbus_cast<LauncherIcon>(argument);
    } else return {};
    if (icon.kind != QStringLiteral("bytes") || icon.data.variant().metaType() != QMetaType::fromType<QByteArray>()) return {};
    const auto bytes = icon.data.variant().toByteArray(); if (bytes.isEmpty() || bytes.size() > 4194304) return {};
    return icon;
}
std::optional<QJsonObject> launcherFrame(const QString &app, const QString &parent,
    const QString &name, const QDBusVariant &icon, const QVariantMap &options) {
    if (name.trimmed().isEmpty() || !boundedText(name, 256)) return {};
    auto frame = miscFrame("launcher", app, parent, "Add Application", options); const auto parsedIcon = launcherIcon(icon);
    if (!frame || !parsedIcon) return {};
    for (const auto *key : {"editable_name", "editable_icon"}) if (options.contains(key) && options.value(key).metaType() != QMetaType::fromType<bool>()) return {};
    if (options.contains("launcher_type") && options.value("launcher_type").metaType() != QMetaType::fromType<quint32>()) return {};
    const auto type = options.value("launcher_type", 1U).toUInt(); if (type != 1 && type != 2) return {};
    const auto target = options.value("target"); if (target.isValid() && (target.metaType() != QMetaType::fromType<QString>() || !boundedText(target.toString(), 2048))) return {};
    frame->insert("title", type == 2 ? "Add Web Application" : "Add Application");
    frame->insert("name", name); frame->insert("icon", QString::fromLatin1(parsedIcon->data.variant().toByteArray().toBase64()));
    frame->insert("target", target.toString()); frame->insert("editable_name", options.value("editable_name", true).toBool());
    // Icon editing is optional in the standard; preserve the exact submitted GBytesIcon.
    return frame;
}
std::optional<QVariantMap> launcherResults(const QJsonObject &request, const QJsonObject &output) {
    const auto name = output.value("name"); if (output.size() != 1 || !name.isString() || name.toString().trimmed().isEmpty() || !boundedText(name.toString(), 256)) return {};
    if (!request.value("editable_name").toBool() && name != request.value("name")) return {};
    const auto bytes = QByteArray::fromBase64(request.value("icon").toString().toLatin1());
    const LauncherIcon icon{QStringLiteral("bytes"), QDBusVariant(bytes)};
    return QVariantMap{{"name", name.toString()}, {"icon", QVariant::fromValue(QDBusVariant(QVariant::fromValue(icon)))}};
}
bool noninteractiveLauncherAllowed(const QString &app) {
    return QStringList{QStringLiteral("org.gnome.Software"), QStringLiteral("org.gnome.SoftwareDevel"), QStringLiteral("io.elementary.appcenter"), QStringLiteral("org.kde.discover")}.contains(app);
}
}
