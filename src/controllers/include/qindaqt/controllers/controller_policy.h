// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/input_actions/input_actions.h>
#include <QJsonObject>
#include <QMap>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace QindaQt::Controllers {
inline constexpr auto Service = "org.qindaqt.Controllers1";
inline constexpr auto Object = "/org/qindaqt/Controllers1";
inline constexpr auto Interface = "org.qindaqt.Controllers1";

using Binding = InputActions::Binding;
struct ButtonPage {
    QString id = QStringLiteral("main");
    QString name = QStringLiteral("Desktop");
    QString color = QStringLiteral("#287DFF");
    bool layer = false;
    QMap<QString, Binding> bindings;
    friend bool operator==(const ButtonPage &, const ButtonPage &) = default;
};
inline constexpr qsizetype MaxConfigBytes = 128 * 1024;
inline constexpr qsizetype MaxStoreBytes = 2 * 1024 * 1024;
inline constexpr qsizetype MaxSnapshotBytes = 4 * 1024 * 1024;
inline constexpr int MaxPages = 12;
inline constexpr int MaxTextBytes = InputActions::MaxTextBytes;
struct Profile {
    bool enabled = true;
    QString pointerStick = QStringLiteral("left");
    double pointerSpeed = 1000;
    double deadzone = 0.18;
    bool touchpad = true;
    bool gyro = false;
    double gyroSpeed = 650;
    QString activePage = QStringLiteral("main");
    QList<ButtonPage> pages{ButtonPage{}};
    friend bool operator==(const Profile &, const Profile &) = default;
};

// Value-only boundary shared by the compositor and Settings. Patches are
// bounded and atomic; no action is a command or executable path (ADR-0347).
QStringList buttonIds();
QStringList actionIds();
// Profiles always contain a valid active page. Page changes release owned
// input before adopting another map; motion preferences belong to Profile.
const ButtonPage &activePage(const Profile &profile);
ButtonPage &activePage(Profile &profile);
using InputActions::bindingJson;
using InputActions::readBinding;
QJsonObject pageJson(const ButtonPage &page);
// DualSense has its own defaults; all other families retain the generic map.
// Saved per-device overrides remain authoritative across default changes.
Profile defaultProfile(const QString &family = {});
QJsonObject profileJson(const Profile &profile);
bool applyPatch(const QJsonObject &patch, Profile &profile, QString &reason);
QPointF stickMotion(QPointF stick, double deadzone, double speed, double seconds);
QPointF gyroMotion(QPointF angularVelocity, double sensitivity, double seconds);
QString defaultFamilyId(const QString &family);
// Serial-backed physical identity ignores transport; empty serial preserves
// the legacy GUID identity. No runtime handles or filesystem access.
QString controllerProfileId(const QString &guid, const QString &serial, quint16 vendor, quint16 product, const QString &family);
} // namespace QindaQt::Controllers
