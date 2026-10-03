// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/profile_store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace QindaQt::Controllers {
ProfileStore::ProfileStore(QString path) : m_path(std::move(path)) {
    for (const auto &family : QStringList{"xbox", "playstation", "nintendo", "generic"}) {
        const auto id = defaultFamilyId(family);
        m_profiles.insert(id, defaultProfile());
        m_descriptions.insert(id, {{"id", id}, {"family", family}, {"template", true},
            {"name", family == "xbox" ? "Xbox defaults" : family == "playstation" ? "PlayStation defaults"
                 : family == "nintendo" ? "Nintendo defaults" : "Other controller defaults"}});
    }
}
bool ProfileStore::load(QString &reason) {
    QFile file(m_path);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 256 * 1024) { reason = "config-unreadable"; return false; }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()
        || doc.object().value("schemaVersion").toInt() != 1
        || !doc.object().value("profiles").isArray()) { reason = "config-invalid"; return false; }
    auto profiles = m_profiles;
    auto descriptions = m_descriptions;
    const auto rows = doc.object().value("profiles").toArray();
    if (rows.size() > 64) { reason = "config-invalid"; return false; }
    for (const auto &row : rows) {
        if (!row.isObject()) { reason = "config-invalid"; return false; }
        const auto obj = row.toObject();
        const auto id = obj.value("id").toString();
        if (id.isEmpty() || id.toUtf8().size() > 160 || !obj.value("config").isObject()) { reason = "config-invalid"; return false; }
        Profile p = defaultProfile();
        if (!applyPatch(obj.value("config").toObject(), p, reason)) return false;
        profiles[id] = p;
        if (obj.value("description").isObject() && !id.startsWith("default:"))
            descriptions[id] = obj.value("description").toObject();
    }
    m_profiles = profiles;
    m_descriptions = descriptions;
    return true;
}
bool ProfileStore::save(QString &reason) const {
    if (m_profiles.size() > 64 || !QDir().mkpath(QFileInfo(m_path).absolutePath())) { reason = "config-unsaved"; return false; }
    QJsonArray rows;
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it)
        rows.append(QJsonObject{{"id", it.key()}, {"config", profileJson(it.value())},
                               {"description", m_descriptions.value(it.key())}});
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    const auto data = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"profiles", rows}}).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) { reason = "config-unsaved"; return false; }
    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}
Profile ProfileStore::profile(const QString &id, const QString &family) const {
    return m_profiles.value(id, m_profiles.value(defaultFamilyId(family), defaultProfile()));
}
void ProfileStore::set(const QString &id, const Profile &p, const QJsonObject &description) {
    m_profiles[id] = p;
    if (!description.isEmpty()) m_descriptions[id] = description;
}
QJsonObject ProfileStore::description(const QString &id) const { return m_descriptions.value(id); }
QStringList ProfileStore::ids() const { return m_profiles.keys(); }
} // namespace QindaQt::Controllers
