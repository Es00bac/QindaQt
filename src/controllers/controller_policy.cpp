// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/controller_policy.h"
#include <QJsonArray>
#include <QCryptographicHash>
#include <QKeySequence>
#include <algorithm>
#include <cmath>

namespace QindaQt::Controllers {
QStringList buttonIds() {
    return {"south", "east", "west", "north", "back", "guide", "start",
            "leftstick", "rightstick", "leftshoulder", "rightshoulder",
            "dpup", "dpdown", "dpleft", "dpright", "misc1", "rightpaddle1",
            "leftpaddle1", "rightpaddle2", "leftpaddle2", "touchpad",
            "misc2", "misc3", "misc4", "misc5", "misc6", "lefttrigger", "righttrigger", "touchpad-touch"};
}
QStringList actionIds() { return InputActions::actionIds(); }
Profile defaultProfile(const QString &family) {
    Profile p;
    for (const auto &button : buttonIds()) activePage(p).bindings.insert(button, {});
    const QMap<QString, QString> defaults{
        {"south", "left-click"}, {"east", "back"}, {"west", "accept"},
        {"north", "overview"}, {"back", "dictate"}, {"guide", "show-desktop"},
        {"start", "launcher"}, {"leftstick", "left-click"},
        {"rightstick", "right-click"}, {"leftshoulder", "previous-window"},
        {"rightshoulder", "next-window"}, {"dpup", "up"}, {"dpdown", "down"},
        {"dpleft", "left"}, {"dpright", "right"}, {"misc1", "screenshot"},
        {"touchpad", "left-click"}, {"lefttrigger", "right-click"},
        {"righttrigger", "left-click"}};
    for (auto it = defaults.begin(); it != defaults.end(); ++it) activePage(p).bindings[it.key()].action = it.value();
    if (family == "dualsense") {
        const QMap<QString, Binding> dualSense{
            {"back", {"back", {}}}, {"dpup", {"shortcut", "Ctrl+K"}},
            {"dpleft", {"shortcut", "Ctrl+C"}}, {"dpright", {"shortcut", "Ctrl+J"}},
            {"dpdown", {"accept", {}}}, {"leftstick", {"previous-window", {}}},
            {"guide", {"launcher", {}}}, {"start", {"back", {}}},
            {"north", {"maximize", {}}}, {"east", {"overview", {}}},
            {"west", {"voice-command", {}}}, {"south", {"dictate", {}}},
            {"rightstick", {"next-window", {}}}, {"misc1", {"next-page", {}}},
            {"touchpad", {"left-click", {}}}, {"touchpad-touch", {"hold-gyro", {}}}, {"rightshoulder", {"left-click", {}}},
            {"righttrigger", {"combination", {}, {"Meta"}, {}, {"left"}, {}, {}, "move"}},
            {"leftshoulder", {"right-click", {}}},
            {"lefttrigger", {"combination", {}, {"Shift", "Meta"}, {}, {"right"}, {}, {}, "move"}}};
        for (auto it = dualSense.begin(); it != dualSense.end(); ++it) activePage(p).bindings[it.key()] = it.value();
        auto navigation = activePage(p); navigation.id = "navigation"; navigation.name = "Navigation"; navigation.color = "#A83DFF";
        const QMap<QString, QString> arrows{{"dpup", "up"}, {"dpdown", "down"}, {"dpleft", "left"}, {"dpright", "right"}};
        for (auto it = arrows.begin(); it != arrows.end(); ++it) navigation.bindings[it.key()] = Binding{it.value()};
        navigation.bindings["north"] = {"accept"}; navigation.bindings["east"] = {"shortcut", "Backspace"};
        navigation.bindings["start"] = {"shortcut", "Tab"};
        navigation.bindings["righttrigger"] = {"combination", {}, {"Shift"}, {}, {}};
        navigation.bindings["lefttrigger"] = {"combination", {}, {"Ctrl"}, {}, {}, {}, {}, "edit"};
        auto move = activePage(p); move.id = "move"; move.name = "Move / resize"; move.color = "#FF8A24"; move.layer = true;
        const QMap<QString, QString> nudges{{"dpup", "pointer-up"}, {"dpdown", "pointer-down"}, {"dpleft", "pointer-left"}, {"dpright", "pointer-right"}};
        for (auto it = nudges.begin(); it != nudges.end(); ++it) move.bindings[it.key()] = Binding{it.value()};
        auto edit = navigation; edit.id = "edit"; edit.name = "Editing"; edit.color = "#24D16B"; edit.layer = true;
        const QMap<QString, QString> letters{{"dpup", "A"}, {"dpdown", "X"}, {"dpleft", "C"}, {"dpright", "V"}, {"north", "Z"}, {"east", "Y"}};
        for (auto it = letters.begin(); it != letters.end(); ++it) edit.bindings[it.key()] = {"combination", {}, {}, {it.value()}, {}};
        p.pages.append(navigation); p.pages.append(move); p.pages.append(edit);
    }
    return p;
}
const ButtonPage &activePage(const Profile &p) {
    for (const auto &page : p.pages) if (page.id == p.activePage) return page;
    Q_UNREACHABLE();
}
ButtonPage &activePage(Profile &p) {
    for (auto &page : p.pages) if (page.id == p.activePage) return page;
    Q_UNREACHABLE();
}
QPointF stickMotion(QPointF s, double deadzone, double speed, double dt) {
    if (!std::isfinite(s.x()) || !std::isfinite(s.y()) || !std::isfinite(dt) || dt <= 0) return {};
    const double length = std::hypot(s.x(), s.y());
    if (length <= deadzone || length == 0) return {};
    const double amount = std::pow((std::min(length, 1.0) - deadzone) / (1.0 - deadzone), 1.7);
    return s / length * (amount * speed * std::min(dt, 0.05));
}
QPointF gyroMotion(QPointF v, double sensitivity, double dt) {
    if (!std::isfinite(v.x()) || !std::isfinite(v.y()) || !std::isfinite(dt) || dt <= 0) return {};
    if (std::abs(v.x()) < 0.025) v.setX(0);
    if (std::abs(v.y()) < 0.025) v.setY(0);
    return v * (sensitivity * std::min(dt, 0.05));
}
QString defaultFamilyId(const QString &family) { return QStringLiteral("default:") + family; }
QString controllerProfileId(const QString &guid, const QString &serial, quint16 vendor, quint16 product, const QString &family) {
    auto stableSerial = serial.trimmed().toLower();
    if (family == "playstation" || family == "dualsense") { stableSerial.remove(':'); stableSerial.remove('-'); }
    const QString identity = stableSerial.isEmpty() ? guid + ':' + serial
        : QString("hardware:%1:%2:%3").arg(vendor).arg(product).arg(stableSerial);
    return "pad:" + QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
}
} // namespace QindaQt::Controllers
