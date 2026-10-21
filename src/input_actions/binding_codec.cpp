// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/input_actions/input_actions.h"
#include <QJsonArray>
#include <QKeySequence>
#include <QRegularExpression>
#include <QSet>

namespace QindaQt::InputActions {
namespace {
bool stringList(const QJsonValue &value, const QStringList &allowed, int limit, QStringList &result) {
    if (value.isUndefined()) return true;
    if (!value.isArray() || value.toArray().size() > limit) return false;
    for (const auto &item : value.toArray()) {
        if (!item.isString() || !allowed.contains(item.toString()) || result.contains(item.toString())) return false;
        result.append(item.toString());
    }
    // Fixed ordering makes shared holds and reverse releases deterministic.
    std::sort(result.begin(), result.end(), [&allowed](const auto &a, const auto &b) { return allowed.indexOf(a) < allowed.indexOf(b); });
    return true;
}
}
Binding asCombination(const Binding &value) {
    if (value.action == "combination") return value;
    Binding b; b.action = "combination";
    if (value.action == "shortcut") {
        const auto seq = QKeySequence::fromString(value.shortcut, QKeySequence::PortableText);
        if (!seq.isEmpty()) {
            const auto mods = seq[0].keyboardModifiers();
            if (mods.testFlag(Qt::ControlModifier)) b.modifiers << "Ctrl";
            if (mods.testFlag(Qt::AltModifier)) b.modifiers << "Alt";
            if (mods.testFlag(Qt::ShiftModifier)) b.modifiers << "Shift";
            if (mods.testFlag(Qt::MetaModifier)) b.modifiers << "Meta";
            b.keys << QKeySequence(seq[0].key()).toString(QKeySequence::PortableText);
        }
    }
    if (value.action == "left-click" || value.action == "left-click-meta-shift") b.mouseButtons << "left";
    if (value.action == "right-click" || value.action == "right-click-meta-shift") b.mouseButtons << "right";
    if (value.action == "middle-click") b.mouseButtons << "middle";
    if (value.action.endsWith("-meta-shift")) b.modifiers = {"Shift", "Meta"};
    return b;
}
QJsonObject bindingJson(const Binding &b) {
    QJsonObject value{{"action", b.action}};
    if (b.action == "shortcut") value["shortcut"] = b.shortcut;
    if (b.action == "combination") {
        value["modifiers"] = QJsonArray::fromStringList(b.modifiers);
        value["keys"] = QJsonArray::fromStringList(b.keys);
        value["mouseButtons"] = QJsonArray::fromStringList(b.mouseButtons);
    }
    if (b.action == "text") value["text"] = b.text;
    if (b.action == "switch-page" || b.action == "hold-page") value["page"] = b.page;
    if (b.action == "macro") value["macro"] = b.macro;
    if (!b.holdPage.isEmpty()) value["holdPage"] = b.holdPage;
    return value;
}
bool readBinding(const QJsonObject &value, Binding &binding, QString &reason) {
    const auto fail = [&reason](const char *s) { reason = QLatin1String(s); return false; };
    if (!value.value("action").isString() || !actionIds().contains(value.value("action").toString())) return fail("invalid-binding");
    Binding b; b.action = value.value("action").toString();
    QStringList fields{"action", "shortcut"}; // Version-1 writers included empty shortcut on every action.
    if (b.action == "combination") fields += {"modifiers", "keys", "mouseButtons", "holdPage"};
    if (b.action == "macro") fields.append("macro");
    if (b.action == "text") fields.append("text");
    if (b.action == "switch-page" || b.action == "hold-page") fields.append("page");
    for (auto it = value.begin(); it != value.end(); ++it)
        if (!fields.contains(it.key()) || (it.key() != "modifiers" && it.key() != "keys" && it.key() != "mouseButtons" && !it->isString())) return fail("invalid-binding");
    if (b.action == "shortcut") {
        const auto text = value.value("shortcut").toString();
        const auto seq = QKeySequence::fromString(text, QKeySequence::PortableText);
        if (text.toUtf8().size() > 64 || seq.count() != 1 || seq[0].key() == Qt::Key_unknown || seq[0].key() == 0) return fail("invalid-shortcut");
        b.shortcut = seq.toString(QKeySequence::PortableText);
    }
    if (b.action == "combination") {
        if (!stringList(value.value("modifiers"), {"Ctrl", "Alt", "Shift", "Meta"}, 4, b.modifiers)
            || !stringList(value.value("mouseButtons"), {"left", "right", "middle", "back", "forward"}, 5, b.mouseButtons)
            || !value.value("keys").isArray() || value.value("keys").toArray().size() > 8) return fail("invalid-combination");
        for (const auto &item : value.value("keys").toArray()) {
            if (!item.isString() || item.toString().toUtf8().size() > 64) return fail("invalid-combination");
            const auto seq = QKeySequence::fromString(item.toString(), QKeySequence::PortableText);
            if (seq.count() != 1 || seq[0].key() == 0 || seq[0].key() == Qt::Key_unknown
                || seq[0].keyboardModifiers() != Qt::NoModifier
                || seq[0].key() == Qt::Key_Control || seq[0].key() == Qt::Key_Alt
                || seq[0].key() == Qt::Key_Shift || seq[0].key() == Qt::Key_Meta || seq[0].key() == Qt::Key_AltGr) return fail("invalid-combination");
            const auto key = seq.toString(QKeySequence::PortableText);
            if (b.keys.contains(key)) return fail("invalid-combination");
            b.keys.append(key);
        }
        if (b.modifiers.isEmpty() && b.keys.isEmpty() && b.mouseButtons.isEmpty()) return fail("invalid-combination");
    }
    if (b.action == "text") {
        b.text = value.value("text").toString();
        if (b.text.isEmpty() || !b.text.isValidUtf16() || b.text.toUtf8().size() > MaxTextBytes) return fail("invalid-text");
        for (const auto ch : b.text)
            if ((ch.unicode() < 32 && ch != '\n' && ch != '\r' && ch != '\t') || ch.unicode() == 127) return fail("invalid-text");
    }
    if (b.action == "combination" && value.contains("holdPage")) {
        b.holdPage = value.value("holdPage").toString();
        if (!b.holdPage.isEmpty() && !QRegularExpression("^[a-zA-Z0-9_-]{1,40}$").match(b.holdPage).hasMatch()) return fail("invalid-page");
    }
    if (b.action == "switch-page" || b.action == "hold-page") {
        b.page = value.value("page").toString();
        if (!QRegularExpression("^[a-zA-Z0-9_-]{1,40}$").match(b.page).hasMatch()) return fail("invalid-page");
    }
    if (b.action == "macro") {
        b.macro = value.value("macro").toString();
        if (!QRegularExpression("^[a-zA-Z0-9_-]{1,40}$").match(b.macro).hasMatch()) return fail("invalid-macro");
    }
    binding = std::move(b); reason.clear(); return true;
}
} // namespace QindaQt::InputActions
