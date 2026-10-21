// SPDX-License-Identifier: GPL-3.0-or-later
#include "macro_endpoint.h"
#include <QJsonDocument>
namespace QindaQt::Controllers {
namespace { QString json(const QJsonObject &obj) { return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)); } }
MacroEndpoint::MacroEndpoint(InputActions::MacroStore &store, ControllerInputDevice &input) : m_store(store), m_input(input) {
    m_ready = m_store.load(m_error);
    connect(&input, &ControllerInputDevice::macroChanged, this, &MacroEndpoint::bump);
}
void MacroEndpoint::bump() { Q_EMIT Changed(++m_revision); }
QString MacroEndpoint::result(bool ok, const QString &reason) const { return json({{"ok", ok}, {"reason", reason}, {"revision", QString::number(m_revision)}}); }
QString MacroEndpoint::GetSnapshot() const {
    QJsonArray rows;
    for (const auto &macro : m_store.values()) {
        const auto status = m_shortcuts.value(macro.id);
        rows.append(QJsonObject{{"config", InputActions::macroJson(macro)}, {"shortcutStatus", status.value("reason")}, {"actualShortcuts", status.value("actual")}});
    }
    return json({{"schemaVersion", 1}, {"revision", QString::number(m_revision)}, {"ready", m_ready}, {"error", m_error}, {"running", m_input.runningMacro()}, {"macros", rows}});
}
QString MacroEndpoint::Apply(const QString &id, const QString &config, qulonglong revision) {
    if (!m_ready) return result(false, "macros-unavailable");
    if (revision != m_revision) return result(false, "revision-stale");
    if (config.toUtf8().size() > 128 * 1024) return result(false, "invalid-macro");
    QJsonParseError error; const auto doc = QJsonDocument::fromJson(config.toUtf8(), &error);
    InputActions::Macro macro; QString reason;
    if (error.error != QJsonParseError::NoError || !doc.isObject() || !InputActions::readMacro(doc.object(), macro, reason) || macro.id != id) return result(false, "invalid-macro");
    const bool shortcutChanged = !m_store.contains(id) || m_store.value(id).shortcut != macro.shortcut;
    if (!m_store.set(macro, reason)) return result(false, reason);
    if (m_input.runningMacro() == id) m_input.cancelMacro();
    m_error.clear(); bump(); Q_EMIT libraryChanged(shortcutChanged ? id : QString{}); return result(true, "ok");
}
QString MacroEndpoint::Reset(const QString &id, qulonglong revision) {
    if (!m_ready || revision != m_revision) return result(false, !m_ready ? "macros-unavailable" : "revision-stale");
    QString reason; if (!m_store.remove(id, reason)) return result(false, reason);
    if (m_input.runningMacro() == id) m_input.cancelMacro();
    m_shortcuts.remove(id); m_error.clear(); bump(); Q_EMIT libraryChanged({}); return result(true, "ok");
}
QString MacroEndpoint::Run(const QString &id, qulonglong revision) {
    if (!m_ready || revision != m_revision) return result(false, !m_ready ? "macros-unavailable" : "revision-stale");
    return result(m_input.runMacro("library/" + id, id, 3000), "run-requested");
}
void MacroEndpoint::Cancel() { m_input.cancelMacro(); }
void MacroEndpoint::shortcutStatus(const QString &id, const QString &reason, const QStringList &actual) {
    const QJsonObject value{{"reason", reason}, {"actual", QJsonArray::fromStringList(actual)}};
    if (!m_store.contains(id) || m_shortcuts.value(id) == value) return;
    m_shortcuts[id] = value; bump();
}
} // namespace QindaQt::Controllers
