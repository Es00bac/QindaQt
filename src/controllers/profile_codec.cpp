// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/controller_policy.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace QindaQt::Controllers {
namespace {
bool readBindings(const QJsonObject &values, QMap<QString, Binding> &bindings, QString &reason) {
    if (values.size() > buttonIds().size()) { reason = "invalid-binding"; return false; }
    for (auto it = values.begin(); it != values.end(); ++it) {
        Binding binding;
        if (!buttonIds().contains(it.key()) || !it->isObject()) { reason = "invalid-binding"; return false; }
        if (!readBinding(it->toObject(), binding, reason)) return false;
        bindings[it.key()] = binding;
    }
    return true;
}
}
QJsonObject pageJson(const ButtonPage &page) {
    QJsonObject bindings;
    for (auto it = page.bindings.begin(); it != page.bindings.end(); ++it) bindings[it.key()] = bindingJson(it.value());
    return {{"id", page.id}, {"name", page.name}, {"color", page.color}, {"layer", page.layer}, {"bindings", bindings}};
}
QJsonObject profileJson(const Profile &p) {
    QJsonArray pages;
    for (const auto &page : p.pages) pages.append(pageJson(page));
    return {{"enabled", p.enabled}, {"pointerStick", p.pointerStick}, {"pointerSpeed", p.pointerSpeed},
        {"deadzone", p.deadzone}, {"touchpad", p.touchpad}, {"gyro", p.gyro}, {"gyroSpeed", p.gyroSpeed},
        {"activePage", p.activePage}, {"pages", pages}};
}
bool applyPatch(const QJsonObject &patch, Profile &p, QString &reason) {
    Profile next = p;
    const auto fail = [&reason](const char *s) { reason = QLatin1String(s); return false; };
    if (QJsonDocument(patch).toJson(QJsonDocument::Compact).size() > MaxConfigBytes) return fail("invalid-config");
    if (patch.contains("pages") && patch.contains("bindings")) return fail("invalid-config");
    for (auto it = patch.begin(); it != patch.end(); ++it) {
        const auto key = it.key();
        if (key == "enabled" || key == "touchpad" || key == "gyro") {
            if (!it->isBool()) return fail("invalid-config");
            if (key == "enabled") next.enabled = it->toBool();
            if (key == "touchpad") next.touchpad = it->toBool();
            if (key == "gyro") next.gyro = it->toBool();
        } else if (key == "pointerStick") {
            if (!it->isString() || !QStringList{"left", "right", "off"}.contains(it->toString())) return fail("invalid-config");
            next.pointerStick = it->toString();
        } else if (key == "pointerSpeed" || key == "deadzone" || key == "gyroSpeed") {
            if (!it->isDouble() || !std::isfinite(it->toDouble())) return fail("invalid-config");
            const double v = it->toDouble();
            if (key == "pointerSpeed" && v >= 100 && v <= 3000) next.pointerSpeed = v;
            else if (key == "gyroSpeed" && v >= 50 && v <= 2000) next.gyroSpeed = v;
            else if (key == "deadzone" && v >= 0.05 && v <= 0.6) next.deadzone = v;
            else return fail("invalid-config");
        } else if (key == "activePage") {
            if (!it->isString()) return fail("invalid-page");
            next.activePage = it->toString();
        } else if (key == "pages") {
            if (!it->isArray() || it->toArray().isEmpty() || it->toArray().size() > MaxPages) return fail("invalid-page");
            QList<ButtonPage> pages; QSet<QString> ids;
            for (const auto &entry : it->toArray()) {
                if (!entry.isObject()) return fail("invalid-page");
                const auto obj = entry.toObject();
                for (const auto &field : obj.keys()) if (!QStringList{"bindings", "color", "id", "name", "layer"}.contains(field)) return fail("invalid-page");
                if ((!obj.value("layer").isUndefined() && !obj.value("layer").isBool())
                    || !obj.value("id").isString() || !obj.value("name").isString()
                    || !obj.value("color").isString() || !obj.value("bindings").isObject()) return fail("invalid-page");
                ButtonPage page; page.id = obj.value("id").toString(); page.name = obj.value("name").toString(); page.color = obj.value("color").toString().toUpper(); page.layer = obj.value("layer").toBool();
                if (!QRegularExpression("^[a-zA-Z0-9_-]{1,40}$").match(page.id).hasMatch() || ids.contains(page.id)
                    || page.name.trimmed().isEmpty() || page.name.size() > 48 || !page.name.isValidUtf16()
                    || !QRegularExpression("^#[0-9A-F]{6}$").match(page.color).hasMatch()) return fail("invalid-page");
                for (const auto ch : page.name) if (ch.unicode() < 32) return fail("invalid-page");
                for (const auto &button : buttonIds()) page.bindings[button] = {};
                if (!readBindings(obj.value("bindings").toObject(), page.bindings, reason)) return false;
                ids.insert(page.id); pages.append(std::move(page));
            }
            next.pages = std::move(pages);
        } else if (key == "bindings") {
            // AGENT-CONTRACT: version-1 patches edit the current page. Full
            // version-1 files migrate into the factory's sole Desktop page.
            if (!it->isObject()) return fail("invalid-binding");
        } else return fail("invalid-config");
    }
    bool hasNormalPage = false; for (const auto &page : next.pages) hasNormalPage |= !page.layer;
    if (!hasNormalPage) return fail("invalid-page");
    QSet<QString> ids;
    for (const auto &page : next.pages) ids.insert(page.id);
    if (!ids.contains(next.activePage)) return fail("invalid-page");
    if (patch.contains("bindings") && !readBindings(patch.value("bindings").toObject(), activePage(next).bindings, reason)) return false;
    for (const auto &page : next.pages) for (const auto &binding : page.bindings)
        if (((binding.action == "switch-page" || binding.action == "hold-page") && !ids.contains(binding.page))
            || (!binding.holdPage.isEmpty() && !ids.contains(binding.holdPage))) return fail("invalid-page");
    if (QJsonDocument(profileJson(next)).toJson(QJsonDocument::Compact).size() > MaxConfigBytes) return fail("invalid-config");
    p = std::move(next); reason.clear(); return true;
}
} // namespace QindaQt::Controllers
