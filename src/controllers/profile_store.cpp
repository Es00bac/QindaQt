// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/profile_store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace QindaQt::Controllers {
ProfileStore::ProfileStore(QString path) : m_path(std::move(path)) {
    for (const auto &family : QStringList{"xbox", "playstation", "dualsense", "nintendo", "generic"}) {
        const auto id = defaultFamilyId(family);
        m_profiles.insert(id, defaultProfile(family));
        m_descriptions.insert(id, {{"id", id}, {"family", family}, {"template", true},
            {"name", family == "xbox" ? "Xbox defaults" : family == "playstation" ? "PlayStation defaults"
                 : family == "dualsense" ? "DualSense defaults"
                 : family == "nintendo" ? "Nintendo defaults" : "Other controller defaults"}});
    }
}
bool ProfileStore::load(QString &reason) {
    QFile file(m_path);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxStoreBytes) { reason = "config-unreadable"; return false; }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()
        || !QList<int>{1, 2}.contains(doc.object().value("schemaVersion").toInt())
        || !doc.object().value("profiles").isArray()) { reason = "config-invalid"; return false; }
    auto profiles = m_profiles;
    auto descriptions = m_descriptions;
    const auto rows = doc.object().value("profiles").toArray();
    if (rows.size() > 64) { reason = "config-invalid"; return false; }
    QSet<QString> seen;
    for (const auto &row : rows) {
        if (!row.isObject()) { reason = "config-invalid"; return false; }
        const auto obj = row.toObject();
        const auto id = obj.value("id").toString();
        if (id.isEmpty() || seen.contains(id) || id.toUtf8().size() > 160 || !obj.value("config").isObject()) { reason = "config-invalid"; return false; }
        seen.insert(id);
        auto description = obj.value("description").toObject();
        // AGENT-CONTRACT: older Controllers1 profiles grouped PS5 with PS3/4.
        // Reclassify known DualSense metadata without changing saved bindings
        // or serial-based IDs, so Reset uses the correct family defaults.
        if (description.value("family") == "playstation"
            && description.value("name").toString().contains("DualSense", Qt::CaseInsensitive))
            description["family"] = "dualsense";
        Profile p = defaultProfile(description.value("family").toString());
        if (doc.object().value("schemaVersion").toInt() == 1) {
            p.pages = {activePage(p)};
            for (auto &binding : p.pages[0].bindings) binding.holdPage.clear();
        }
        if (!applyPatch(obj.value("config").toObject(), p, reason)) return false;
        profiles[id] = p;
        if (obj.value("description").isObject() && !id.startsWith("default:"))
            descriptions[id] = description;
    }
    if (profiles.size() > 64) { reason = "config-invalid"; return false; }
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
    const auto data = QJsonDocument(QJsonObject{{"schemaVersion", 2}, {"profiles", rows}}).toJson(QJsonDocument::Compact);
    if (data.size() > MaxStoreBytes || !file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(data) != data.size() || !file.commit()) { reason = "config-unsaved"; return false; }
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
