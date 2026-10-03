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
            "misc2", "misc3", "misc4", "misc5", "misc6", "lefttrigger", "righttrigger"};
}
QStringList actionIds() {
    return {"none", "dictate", "left-click", "right-click", "middle-click",
            "accept", "back", "up", "down", "left", "right", "overview",
            "launcher", "next-window", "previous-window", "maximize",
            "close-window", "desktop-left", "desktop-right", "show-desktop",
            "screenshot", "shortcut"};
}
Profile defaultProfile() {
    Profile p;
    for (const auto &button : buttonIds()) p.bindings.insert(button, {});
    const QMap<QString, QString> defaults{
        {"south", "left-click"}, {"east", "back"}, {"west", "accept"},
        {"north", "overview"}, {"back", "dictate"}, {"guide", "show-desktop"},
        {"start", "launcher"}, {"leftstick", "left-click"},
        {"rightstick", "right-click"}, {"leftshoulder", "previous-window"},
        {"rightshoulder", "next-window"}, {"dpup", "up"}, {"dpdown", "down"},
        {"dpleft", "left"}, {"dpright", "right"}, {"misc1", "screenshot"},
        {"touchpad", "left-click"}, {"lefttrigger", "right-click"},
        {"righttrigger", "left-click"}};
    for (auto it = defaults.begin(); it != defaults.end(); ++it) p.bindings[it.key()].action = it.value();
    return p;
}
QJsonObject profileJson(const Profile &p) {
    QJsonObject bindings;
    for (auto it = p.bindings.begin(); it != p.bindings.end(); ++it)
        bindings.insert(it.key(), QJsonObject{{"action", it->action}, {"shortcut", it->shortcut}});
    return {{"enabled", p.enabled}, {"pointerStick", p.pointerStick},
            {"pointerSpeed", p.pointerSpeed}, {"deadzone", p.deadzone},
            {"touchpad", p.touchpad}, {"gyro", p.gyro}, {"gyroSpeed", p.gyroSpeed},
            {"bindings", bindings}};
}
bool applyPatch(const QJsonObject &patch, Profile &p, QString &reason) {
    Profile next = p;
    const auto fail = [&reason](const QString &s) { reason = s; return false; };
    if (patch.size() > 8) return fail("invalid-config");
    for (auto it = patch.begin(); it != patch.end(); ++it) {
        const auto key = it.key();
        if (key == "enabled" || key == "touchpad" || key == "gyro") {
            if (!it->isBool()) return fail("invalid-config");
            if (key == "enabled") next.enabled = it->toBool();
            if (key == "touchpad") next.touchpad = it->toBool();
            if (key == "gyro") next.gyro = it->toBool();
        } else if (key == "pointerStick") {
            if (!it->isString() || !QStringList{"left", "right", "off"}.contains(it->toString()))
                return fail("invalid-config");
            next.pointerStick = it->toString();
        } else if (key == "pointerSpeed" || key == "deadzone" || key == "gyroSpeed") {
            if (!it->isDouble() || !std::isfinite(it->toDouble())) return fail("invalid-config");
            const double v = it->toDouble();
            if (key == "pointerSpeed" && v >= 100 && v <= 3000) next.pointerSpeed = v;
            else if (key == "gyroSpeed" && v >= 50 && v <= 2000) next.gyroSpeed = v;
            else if (key == "deadzone" && v >= 0.05 && v <= 0.6) next.deadzone = v;
            else return fail("invalid-config");
        } else if (key == "bindings") {
            if (!it->isObject()) return fail("invalid-binding");
            const auto bindings = it->toObject();
            if (bindings.size() > buttonIds().size()) return fail("invalid-binding");
            for (auto b = bindings.begin(); b != bindings.end(); ++b) {
                if (!buttonIds().contains(b.key()) || !b->isObject()) return fail("invalid-binding");
                const auto value = b->toObject();
                if (value.size() > 2 || !value.value("action").isString()
                    || !actionIds().contains(value.value("action").toString())) return fail("invalid-binding");
                for (const auto &field : value.keys())
                    if (field != "action" && field != "shortcut") return fail("invalid-binding");
                Binding binding{value.value("action").toString(), value.value("shortcut").toString()};
                if (binding.action == "shortcut") {
                    const auto seq = QKeySequence::fromString(binding.shortcut, QKeySequence::PortableText);
                    if (binding.shortcut.toUtf8().size() > 64 || seq.count() != 1
                        || seq[0].key() == Qt::Key_unknown || seq[0].key() == 0)
                        return fail("invalid-shortcut");
                    binding.shortcut = seq.toString(QKeySequence::PortableText);
                } else binding.shortcut.clear();
                next.bindings[b.key()] = binding;
            }
        } else return fail("invalid-config");
    }
    p = next;
    reason.clear();
    return true;
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
    if (family == "playstation") { stableSerial.remove(':'); stableSerial.remove('-'); }
    const QString identity = stableSerial.isEmpty() ? guid + ':' + serial
        : QString("hardware:%1:%2:%3").arg(vendor).arg(product).arg(stableSerial);
    return "pad:" + QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
}
} // namespace QindaQt::Controllers
