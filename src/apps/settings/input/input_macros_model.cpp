// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/input_macros_model.h"
#include <qindaqt/input_actions/input_actions.h>
#include <QJsonDocument>
#include <QKeySequence>
#include <QSet>
#include <QUuid>
namespace QindaQt::Apps::SettingsInput {
InputMacrosModel::InputMacrosModel(JsonInputPort &port, QObject *parent) : QObject(parent), m_port(port) {
    connect(&port, &JsonInputPort::snapshotReceived, this, &InputMacrosModel::receive);
    connect(&port, &JsonInputPort::unavailable, this, [this] { m_available = false; m_busy = false; m_macros.clear(); m_revision = 0; m_running.clear(); m_error = tr("Input macros cannot reach the desktop yet."); Q_EMIT viewChanged(); });
    connect(&port, &JsonInputPort::completed, this, [this](bool ok, const QString &reason) {
        m_busy = false;
        m_error = ok ? QString{} : reason == "revision-stale" ? tr("The macro library changed. Try the edit again.")
            : reason == "macros-unsaved" ? tr("The macro could not be saved.") : tr("The macro request could not be completed.");
        Q_EMIT viewChanged();
    });
}
void InputMacrosModel::receive(const QJsonObject &snapshot) {
    bool valid = false; const auto revision = snapshot.value("revision").toString().toULongLong(&valid);
    if (snapshot.value("schemaVersion").toInt() != 1 || !valid || !revision || !snapshot.value("macros").isArray() || snapshot.value("macros").toArray().size() > InputActions::MaxMacros) {
        m_available = false; m_error = tr("The desktop returned an invalid macro library."); Q_EMIT viewChanged(); return;
    }
    QVariantList rows; QSet<QString> ids;
    for (const auto &entry : snapshot.value("macros").toArray()) {
        const auto row = entry.toObject(); InputActions::Macro macro; QString reason;
        if (!entry.isObject() || !row.value("config").isObject() || !InputActions::readMacro(row.value("config").toObject(), macro, reason) || ids.contains(macro.id)) {
            m_available = false; m_error = tr("The desktop returned an invalid macro."); Q_EMIT viewChanged(); return;
        }
        ids.insert(macro.id); auto value = InputActions::macroJson(macro).toVariantMap();
        value["shortcutStatus"] = row.value("shortcutStatus").toString();
        value["actualShortcuts"] = row.value("actualShortcuts").toArray().toVariantList(); rows.append(value);
    }
    m_macros = rows; m_revision = revision; m_running = snapshot.value("running").toString();
    m_available = snapshot.value("ready").toBool();
    if (!m_available) m_error = tr("The saved macro library could not be loaded. Existing files have been preserved.");
    Q_EMIT viewChanged();
}
void InputMacrosModel::refresh() { m_port.refresh(); }
QString InputMacrosModel::fingerprint(const QString &id) const {
    for (const auto &value : m_macros) if (value.toMap().value("id").toString() == id) {
        auto o = QJsonObject::fromVariantMap(value.toMap()); o.remove("shortcutStatus"); o.remove("actualShortcuts");
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    return {};
}
bool InputMacrosModel::save(const QVariantMap &value, const QString &expected) {
    const auto id = value.value("id").toString();
    if (!m_available || m_busy || fingerprint(id) != expected || !macroError(value).isEmpty()) return false;
    m_busy = true; m_error.clear(); Q_EMIT viewChanged(); m_port.apply(id, QJsonObject::fromVariantMap(value), m_revision); return true;
}
bool InputMacrosModel::remove(const QString &id) {
    if (!m_available || m_busy || fingerprint(id).isEmpty()) return false;
    m_busy = true; Q_EMIT viewChanged(); m_port.reset(id, m_revision); return true;
}
bool InputMacrosModel::run(const QString &id) {
    if (!m_available || m_busy || fingerprint(id).isEmpty()) return false;
    m_busy = true; m_error.clear(); Q_EMIT viewChanged(); m_port.run(id, m_revision); return true;
}
void InputMacrosModel::cancel() { m_port.cancelRun(); }
QString InputMacrosModel::macroError(const QVariantMap &value) const {
    InputActions::Macro macro; QString reason;
    return InputActions::readMacro(QJsonObject::fromVariantMap(value), macro, reason) ? QString{} : tr("Use 1–64 valid steps, pair every hold with a release, and keep delays below one minute in total. Text blocks allow 4,096 UTF-8 bytes each.");
}
QString InputMacrosModel::stepError(const QVariantMap &value) const {
    InputActions::MacroStep step; QString reason;
    return InputActions::readStep(QJsonObject::fromVariantMap(value), step, reason) ? QString{} : tr("Choose a valid action and values for this step.");
}
QVariantMap InputMacrosModel::combination(const QVariantMap &value) const {
    InputActions::Binding binding; QString reason;
    InputActions::readBinding(QJsonObject::fromVariantMap(value), binding, reason);
    return InputActions::bindingJson(InputActions::asCombination(binding)).toVariantMap();
}
QString InputMacrosModel::bindingError(const QVariantMap &value) const {
    InputActions::Binding b; QString reason;
    return InputActions::readBinding(QJsonObject::fromVariantMap(value), b, reason) ? QString{} : tr("Choose a valid key, modifier or mouse-button combination.");
}
QString InputMacrosModel::portableKey(int key) const { return QKeySequence(key).toString(QKeySequence::PortableText); }
QString InputMacrosModel::sequenceText(int key) const { return QKeySequence(key).toString(QKeySequence::NativeText); }
int InputMacrosModel::sequenceKey(const QString &text) const { const auto keys = QKeySequence::fromString(text, QKeySequence::PortableText); return keys.isEmpty() ? 0 : keys[0].toCombined(); }
QVariantList InputMacrosModel::actions() const {
    const QList<QPair<QString, QString>> labels{{"combination", tr("Custom keys + mouse")}, {"left-click", tr("Left click")}, {"right-click", tr("Right click")}, {"middle-click", tr("Middle click")},
        {"accept", tr("Enter")}, {"back", tr("Escape")}, {"up", tr("Up arrow")}, {"down", tr("Down arrow")}, {"left", tr("Left arrow")}, {"right", tr("Right arrow")},
        {"overview", tr("Window overview")}, {"launcher", tr("Application launcher")}, {"next-window", tr("Next window")}, {"previous-window", tr("Previous window")},
        {"maximize", tr("Maximize / restore")}, {"close-window", tr("Close window")}, {"desktop-left", tr("Previous workspace")}, {"desktop-right", tr("Next workspace")}, {"show-desktop", tr("Show desktop")}, {"screenshot", tr("Screenshot")}};
    QVariantList rows; for (const auto &[value, label] : labels) rows.append(QVariantMap{{"value", value}, {"label", label}}); return rows;
}
QString InputMacrosModel::stepSummary(const QVariantMap &value) const {
    const auto kind = value.value("kind").toString(); const auto b = value.value("binding").toMap();
    if (kind == "wait") return tr("Wait %1 ms").arg(value.value("milliseconds").toInt());
    if (kind == "move") return tr("Move pointer: %1, %2 px").arg(value.value("x").toDouble()).arg(value.value("y").toDouble());
    if (kind == "scroll") return tr("Scroll: %1, %2").arg(value.value("x").toDouble()).arg(value.value("y").toDouble());
    if (kind == "text") return tr("Insert text · %1 characters").arg(b.value("text").toString().toUcs4().size());
    if (kind == "release") return tr("Release hold %1").arg(value.value("slot").toInt() + 1);
    QString label;
    for (const auto &row : actions()) if (row.toMap().value("value") == b.value("action")) label = row.toMap().value("label").toString();
    if (b.value("action") == "combination") label = (b.value("modifiers").toStringList() + b.value("keys").toStringList() + b.value("mouseButtons").toStringList()).join(" + ");
    if (b.value("action") == "shortcut") label = b.value("shortcut").toString();
    return kind == "press" ? tr("Hold %1: %2").arg(value.value("slot").toInt() + 1).arg(label) : tr("Tap %1 (%2 ms)").arg(label).arg(value.value("milliseconds").toInt());
}
QVariantMap InputMacrosModel::create(const QString &preset) const {
    InputActions::Macro macro; macro.id = QUuid::createUuid().toString(QUuid::Id128); macro.name = tr("New macro");
    InputActions::MacroStep step;
    if (preset == "drag") {
        macro.name = tr("Move window right"); step.kind = "press"; step.binding = {"combination", {}, {"Meta"}, {}, {"left"}}; macro.steps << step;
        step = {}; step.kind = "move"; step.x = 100; macro.steps << step;
        step = {}; step.kind = "release"; macro.steps << step;
    } else if (preset == "copy-paste") {
        macro.name = tr("Copy and paste"); step.binding = {"shortcut", "Ctrl+C"}; macro.steps << step;
        step = {}; step.kind = "wait"; step.milliseconds = 150; macro.steps << step;
        step = {}; step.binding = {"shortcut", "Ctrl+V"}; macro.steps << step;
    } else { step.kind = "text"; step.binding = {"text", {}, {}, {}, {}, tr("Your text")}; macro.steps << step; }
    return InputActions::macroJson(macro).toVariantMap();
}
} // namespace QindaQt::Apps::SettingsInput
