// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_ui.h"
#include <QDBusVariant>
#include <QJsonArray>
#include <tuple>
#include <optional>
namespace QindaQt::Services::Portal {
using UsbDevices = QList<std::tuple<QString, QVariantMap, QVariantMap>>;
using UsbSelections = QList<std::pair<QString, QVariantMap>>;
struct LauncherIcon { QString kind; QDBusVariant data; };
QDBusArgument &operator<<(QDBusArgument &, const LauncherIcon &);
const QDBusArgument &operator>>(const QDBusArgument &, LauncherIcon &);
void registerMiscTypes();
bool boundedText(const QString &, qsizetype limit = 4096);
std::optional<QJsonObject> miscFrame(const QString &kind, const QString &app,
    const QString &parent, const QString &title, const QVariantMap &options);
std::optional<QJsonArray> usbDevicesFrame(const UsbDevices &);
std::optional<UsbSelections> usbResults(const UsbDevices &, const QJsonObject &);
std::optional<LauncherIcon> launcherIcon(const QDBusVariant &);
std::optional<QJsonObject> launcherFrame(const QString &, const QString &,
    const QString &, const QDBusVariant &, const QVariantMap &);
std::optional<QVariantMap> launcherResults(const QJsonObject &, const QJsonObject &);
bool noninteractiveLauncherAllowed(const QString &);
struct AccountInformation { QString id, name, image, defaultImage; };
AccountInformation localAccountInformation();
}
Q_DECLARE_METATYPE(QindaQt::Services::Portal::UsbDevices)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::UsbSelections)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::LauncherIcon)
