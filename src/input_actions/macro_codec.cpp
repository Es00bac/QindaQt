// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/input_actions/input_actions.h"
#include <QJsonDocument>
#include <QKeySequence>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace QindaQt::InputActions {
namespace {
bool fields(const QJsonObject &o, const QStringList &allowed) {
    for (const auto &key : o.keys()) if (!allowed.contains(key)) return false;
    return true;
}
bool integer(const QJsonValue &v, int low, int high) { return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() == v.toInt(-999999) && v.toInt() >= low && v.toInt() <= high; }
}
QJsonObject stepJson(const MacroStep &s) {
    QJsonObject o{{"kind", s.kind}};
    if (s.kind == "tap" || s.kind == "press" || s.kind == "text") o["binding"] = bindingJson(s.binding);
    if (s.kind == "press" || s.kind == "release") o["slot"] = s.slot;
    if (s.kind == "tap" || s.kind == "wait") o["milliseconds"] = s.milliseconds;
    if (s.kind == "move" || s.kind == "scroll") { o["x"] = s.x; o["y"] = s.y; }
    return o;
}
bool readStep(const QJsonObject &o, MacroStep &result, QString &reason) {
    const auto fail = [&reason] { reason = "invalid-macro-step"; return false; };
    if (!o.value("kind").isString()) return fail();
    MacroStep s; s.kind = o.value("kind").toString();
    QStringList allowed{"kind"};
    if (s.kind == "tap" || s.kind == "press" || s.kind == "text") {
        allowed << "binding";
        if (!o.value("binding").isObject() || !readBinding(o.value("binding").toObject(), s.binding, reason)) return fail();
        if (s.kind == "text" ? s.binding.action != "text" : s.binding.action == "text") return fail();
        if (QStringList{"macro", "hold-page", "hold-gyro", "toggle-gyro", "switch-page", "next-page", "previous-page", "dictate", "voice-command", "none"}.contains(s.binding.action)
            || !s.binding.holdPage.isEmpty()) return fail();
        if (s.kind == "press" && !QStringList{"combination", "shortcut", "left-click", "right-click", "middle-click", "left-click-meta-shift", "right-click-meta-shift"}.contains(s.binding.action)) return fail();
    }
    if (s.kind == "press" || s.kind == "release") { allowed << "slot"; if (!integer(o.value("slot"), 0, 7)) return fail(); s.slot = o.value("slot").toInt(); }
    if (s.kind == "tap" || s.kind == "wait") { allowed << "milliseconds"; if (!integer(o.value("milliseconds"), s.kind == "tap" ? 1 : 0, s.kind == "tap" ? 2000 : 10000)) return fail(); s.milliseconds = o.value("milliseconds").toInt(); }
    if (s.kind == "move" || s.kind == "scroll") {
        allowed << "x" << "y";
        if (!o.value("x").isDouble() || !o.value("y").isDouble()) return fail();
        s.x = o.value("x").toDouble(); s.y = o.value("y").toDouble();
        if (!std::isfinite(s.x) || !std::isfinite(s.y) || std::abs(s.x) > 4000 || std::abs(s.y) > 4000) return fail();
    }
    if (!QStringList{"tap", "press", "release", "text", "wait", "move", "scroll"}.contains(s.kind) || !fields(o, allowed)) return fail();
    result = s; reason.clear(); return true;
}
QJsonObject macroJson(const Macro &m) {
    QJsonArray steps; for (const auto &step : m.steps) steps.append(stepJson(step));
    return {{"id", m.id}, {"name", m.name}, {"shortcut", m.shortcut}, {"mouseModifiers", QJsonArray::fromStringList(m.mouseModifiers)}, {"mouseButton", m.mouseButton}, {"steps", steps}};
}
bool readMacro(const QJsonObject &o, Macro &result, QString &reason) {
    const auto fail = [&reason] { reason = "invalid-macro"; return false; };
    if (!fields(o, {"id", "name", "shortcut", "mouseModifiers", "mouseButton", "steps"})) return fail();
    Macro m;
    for (const auto &key : QStringList{"id", "name", "shortcut", "mouseButton"}) if (!o.value(key).isString()) return fail();
    m.id = o.value("id").toString(); m.name = o.value("name").toString(); m.shortcut = o.value("shortcut").toString(); m.mouseButton = o.value("mouseButton").toString();
    if (!QRegularExpression("^[a-zA-Z0-9_-]{1,40}$").match(m.id).hasMatch() || m.name.trimmed().isEmpty() || m.name.size() > 80 || !m.name.isValidUtf16()) return fail();
    for (const auto ch : m.name) if (ch.unicode() < 32) return fail();
    if (!m.shortcut.isEmpty()) {
        const auto seq = QKeySequence::fromString(m.shortcut, QKeySequence::PortableText);
        if (m.shortcut.toUtf8().size() > 64 || seq.count() != 1 || seq[0].key() == 0 || seq[0].key() == Qt::Key_unknown) return fail();
        m.shortcut = seq.toString(QKeySequence::PortableText);
    }
    Binding mouse;
    const QJsonObject mouseValue{{"action", "combination"}, {"modifiers", o.value("mouseModifiers")}, {"keys", QJsonArray{}}, {"mouseButtons", m.mouseButton.isEmpty() ? QJsonArray{} : QJsonArray{m.mouseButton}}};
    if (!m.mouseButton.isEmpty()) {
        if (!readBinding(mouseValue, mouse, reason)) return fail();
        m.mouseModifiers = mouse.modifiers;
    } else if (!o.value("mouseModifiers").isArray() || !o.value("mouseModifiers").toArray().isEmpty()) return fail();
    if (!o.value("steps").isArray() || o.value("steps").toArray().isEmpty() || o.value("steps").toArray().size() > MaxSteps) return fail();
    QSet<int> held; int duration = 0;
    for (const auto &value : o.value("steps").toArray()) {
        MacroStep step;
        if (!value.isObject() || !readStep(value.toObject(), step, reason)) return fail();
        if (step.kind == "press") { if (held.contains(step.slot)) return fail(); held.insert(step.slot); }
        if (step.kind == "release") { if (!held.remove(step.slot)) return fail(); }
        if (step.kind == "tap" || step.kind == "wait") duration += step.milliseconds;
        m.steps.append(step);
    }
    // Every hold has an explicit release; cancel still releases all owned input.
    if (!held.isEmpty() || duration > 60000 || QJsonDocument(o).toJson(QJsonDocument::Compact).size() > MaxMacroBytes) return fail();
    result = std::move(m); reason.clear(); return true;
}
} // namespace QindaQt::InputActions
