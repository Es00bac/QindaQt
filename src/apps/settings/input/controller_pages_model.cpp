// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controllers_model.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QKeySequence>
#include <QUuid>

namespace QindaQt::Apps::SettingsInput {
QString ControllersModel::pageSwitcher() const {
    for (const auto &button : Controllers::buttonIds()) {
        bool common = !pages().isEmpty();
        for (const auto &page : pages())
            common = common && page.toMap().value("bindings").toMap().value(button).toMap().value("action").toString() == "next-page";
        if (common) return button;
    }
    return {};
}
bool ControllersModel::setPageSwitcher(const QString &button) {
    if (!Controllers::buttonIds().contains(button)) return false;
    auto list = QJsonArray::fromVariantList(pages());
    for (qsizetype i = 0; i < list.size(); ++i) {
        auto page = list[i].toObject(); auto bindings = page.value("bindings").toObject();
        bindings[button] = QJsonObject{{"action", "next-page"}}; page["bindings"] = bindings; list[i] = page;
    }
    return submit({{"pages", list}});
}
QVariantList ControllersModel::pages() const { return selected().value("config").toMap().value("pages").toList(); }
QVariantMap ControllersModel::currentPage() const {
    const auto id = selected().value("config").toMap().value("activePage").toString();
    for (const auto &entry : pages()) if (entry.toMap().value("id").toString() == id) return entry.toMap();
    return {};
}
QString ControllersModel::portableKey(int key) const { return QKeySequence(key).toString(QKeySequence::PortableText); }
QString ControllersModel::bindingError(const QVariantMap &value) const {
    Controllers::Binding binding; QString reason;
    if (!Controllers::readBinding(QJsonObject::fromVariantMap(value), binding, reason)) {
        if (reason == "invalid-text") return tr("Enter a text block of up to 4,096 UTF-8 bytes.");
        if (reason == "invalid-combination") return tr("Choose modifiers, keys or mouse buttons. Use up to eight different keys.");
        return tr("Choose a valid binding.");
    }
    if (binding.action == "switch-page" || binding.action == "hold-page" || !binding.holdPage.isEmpty()) {
        for (const auto &page : pages()) if (page.toMap().value("id").toString() == (binding.holdPage.isEmpty() ? binding.page : binding.holdPage)) return {};
        return tr("Choose an existing button profile.");
    }
    return {};
}
bool ControllersModel::saveBinding(const QString &button, const QVariantMap &binding, const QString &controllerId, const QString &pageId) {
    if (controllerId != selectedId() || pageId != currentPage().value("id").toString() || !bindingError(binding).isEmpty()) return false;
    return submit({{"bindings", QJsonObject{{button, QJsonObject::fromVariantMap(binding)}}}});
}
QVariantMap ControllersModel::combination(const QVariantMap &value) const {
    auto json = QJsonObject::fromVariantMap(value);
    for (const auto &key : QStringList{"id", "label", "summary"}) json.remove(key);
    InputActions::Binding binding; QString reason;
    InputActions::readBinding(json, binding, reason);
    return InputActions::bindingJson(InputActions::asCombination(binding)).toVariantMap();
}
QString ControllersModel::bindingSummary(const QVariantMap &value) const {
    const auto action = value.value("action").toString();
    if (action == "combination") {
        auto parts = value.value("modifiers").toStringList();
        for (const auto &key : value.value("keys").toStringList()) parts << QKeySequence::fromString(key, QKeySequence::PortableText).toString(QKeySequence::NativeText);
        const QMap<QString, QString> names{{"left", tr("Left click")}, {"right", tr("Right click")}, {"middle", tr("Middle click")}, {"back", tr("Mouse back")}, {"forward", tr("Mouse forward")}};
        for (const auto &button : value.value("mouseButtons").toStringList()) parts << names.value(button);
        return parts.join(" + ");
    }
    if (action == "shortcut") return QKeySequence::fromString(value.value("shortcut").toString(), QKeySequence::PortableText).toString(QKeySequence::NativeText);
    if (action == "text") return tr("Text block · %1 characters").arg(value.value("text").toString().toUcs4().size());
    if (action == "switch-page") for (const auto &page : pages())
        if (page.toMap().value("id").toString() == value.value("page").toString()) return tr("Switch to %1").arg(page.toMap().value("name").toString());
    for (const auto &entry : actions()) if (entry.toMap().value("value").toString() == action) return entry.toMap().value("label").toString();
    return tr("No desktop action");
}
bool ControllersModel::addPage(const QString &name, const QString &color, bool copyCurrent, bool layer) {
    auto list = QJsonArray::fromVariantList(pages());
    if (list.size() >= Controllers::MaxPages) return false;
    auto page = copyCurrent ? QJsonObject::fromVariantMap(currentPage()) : Controllers::pageJson(Controllers::ButtonPage{});
    const auto id = QUuid::createUuid().toString(QUuid::Id128);
    page["id"] = id; page["name"] = name; page["color"] = color; page["layer"] = layer;
    list.append(page);
    return submit({{"pages", list}, {"activePage", id}});
}
bool ControllersModel::updatePage(const QString &name, const QString &color, bool layer) {
    auto list = QJsonArray::fromVariantList(pages()); const auto id = currentPage().value("id").toString();
    for (qsizetype i = 0; i < list.size(); ++i) {
        auto page = list[i].toObject(); if (page.value("id").toString() != id) continue;
        page["name"] = name; page["color"] = color; page["layer"] = layer; list[i] = page;
    }
    return submit({{"pages", list}});
}
bool ControllersModel::removePage() {
    auto list = QJsonArray::fromVariantList(pages()); if (list.size() < 2) return false;
    const auto id = currentPage().value("id").toString();
    for (qsizetype i = 0; i < list.size(); ++i) if (list[i].toObject().value("id").toString() == id) { list.removeAt(i); break; }
    for (qsizetype i = 0; i < list.size(); ++i) {
        auto page = list[i].toObject(); auto bindings = page.value("bindings").toObject();
        for (const auto &button : bindings.keys()) {
            const auto binding = bindings.value(button).toObject();
            if ((binding.value("action") == "switch-page" || binding.value("action") == "hold-page") && binding.value("page") == id) bindings[button] = QJsonObject{{"action", "none"}};
            else if (binding.value("holdPage") == id) { auto updated = binding; updated.remove("holdPage"); bindings[button] = updated; }
        }
        page["bindings"] = bindings; list[i] = page;
    }
    return submit({{"pages", list}, {"activePage", list.first().toObject().value("id")}});
}
} // namespace QindaQt::Apps::SettingsInput
