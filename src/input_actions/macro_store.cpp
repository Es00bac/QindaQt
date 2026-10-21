// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/input_actions/macro_store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
namespace QindaQt::InputActions {
MacroStore::MacroStore(QString path) : m_path(std::move(path)) {}
bool MacroStore::load(QString &reason) {
    QFile file(m_path); if (!file.exists()) { reason.clear(); return true; }
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxLibraryBytes) { reason = "macros-unreadable"; return false; }
    QJsonParseError parse; const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject() || doc.object().value("schemaVersion").toInt() != 1
        || !doc.object().value("macros").isArray() || doc.object().value("macros").toArray().size() > MaxMacros) { reason = "macros-invalid"; return false; }
    QMap<QString, Macro> macros;
    for (const auto &value : doc.object().value("macros").toArray()) {
        Macro macro;
        if (!value.isObject() || !readMacro(value.toObject(), macro, reason) || macros.contains(macro.id)) { reason = "macros-invalid"; return false; }
        macros[macro.id] = macro;
    }
    m_macros = std::move(macros); reason.clear(); return true;
}
bool MacroStore::save(const QMap<QString, Macro> &macros, QString &reason) const {
    if (macros.size() > MaxMacros) { reason = "macros-full"; return false; }
    QJsonArray rows; for (const auto &macro : macros) rows.append(macroJson(macro));
    const auto bytes = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"macros", rows}}).toJson(QJsonDocument::Compact);
    QSaveFile file(m_path); file.setDirectWriteFallback(false);
    if (bytes.size() > MaxLibraryBytes || !QDir().mkpath(QFileInfo(m_path).absolutePath()) || !file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) || file.write(bytes) != bytes.size() || !file.commit()) { reason = "macros-unsaved"; return false; }
    reason.clear(); return true;
}
bool MacroStore::set(const Macro &macro, QString &reason) {
    Macro valid; if (!readMacro(macroJson(macro), valid, reason)) return false;
    auto macros = m_macros; macros[valid.id] = valid;
    if (!save(macros, reason)) return false;
    m_macros = std::move(macros); return true;
}
bool MacroStore::remove(const QString &id, QString &reason) {
    if (!m_macros.contains(id)) { reason = "unknown-macro"; return false; }
    auto macros = m_macros; macros.remove(id);
    if (!save(macros, reason)) return false;
    m_macros = std::move(macros); return true;
}
} // namespace QindaQt::InputActions
