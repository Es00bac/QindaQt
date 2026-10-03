// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace QindaQt::Controllers {
inline constexpr auto Service = "org.qindaqt.Controllers1";
inline constexpr auto Object = "/org/qindaqt/Controllers1";
inline constexpr auto Interface = "org.qindaqt.Controllers1";

struct Binding {
    QString action = QStringLiteral("none");
    QString shortcut;
    friend bool operator==(const Binding &, const Binding &) = default;
};
struct Profile {
    bool enabled = true;
    QString pointerStick = QStringLiteral("left");
    double pointerSpeed = 1000;
    double deadzone = 0.18;
    bool touchpad = true;
    bool gyro = false;
    double gyroSpeed = 650;
    QMap<QString, Binding> bindings;
    friend bool operator==(const Profile &, const Profile &) = default;
};

// Value-only boundary shared by the compositor and Settings. Patches are
// bounded and atomic; no action is a command or executable path (ADR-0347).
QStringList buttonIds();
QStringList actionIds();
Profile defaultProfile();
QJsonObject profileJson(const Profile &profile);
bool applyPatch(const QJsonObject &patch, Profile &profile, QString &reason);
QPointF stickMotion(QPointF stick, double deadzone, double speed, double seconds);
QPointF gyroMotion(QPointF angularVelocity, double sensitivity, double seconds);
QString defaultFamilyId(const QString &family);
} // namespace QindaQt::Controllers
