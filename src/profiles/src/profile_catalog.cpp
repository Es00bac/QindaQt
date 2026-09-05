// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_catalog.h"

#include "qindaqt/profiles/profile_loader.h"

#include <QSet>
#include <algorithm>

namespace QindaQt::Profiles {

ProfileCatalog::ProfileCatalog(QObject *parent)
    : QObject(parent)
{
}

QVariantList ProfileCatalog::items() const
{
    QVariantList values;
    values.reserve(m_profiles.size());
    for (const auto &profile : m_profiles) {
        values.append(profile.toVariantMap());
    }
    return values;
}

QVariantMap ProfileCatalog::current() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_profiles.size()) {
        return {};
    }
    return m_profiles.at(m_currentIndex).toVariantMap();
}

int ProfileCatalog::currentIndex() const { return m_currentIndex; }
const QVector<LayoutProfile> &ProfileCatalog::profiles() const { return m_profiles; }

bool ProfileCatalog::loadDirectory(const QString &path, QString *error)
{
    return loadDirectories({path}, error);
}

bool ProfileCatalog::loadDirectories(const QStringList &paths, QString *error)
{
    QVector<LayoutProfile> loaded;
    for (const QString &path : paths) {
        QSet<QString> directoryIds;
        for (const auto &result : ProfileLoader::fromDirectory(path)) {
            if (!result.ok || directoryIds.contains(result.profile.id)) {
                if (error) {
                    *error = !result.ok ? result.error.diagnostic()
                        : QStringLiteral("duplicate profile id: %1").arg(result.profile.id);
                }
                return false;
            }
            directoryIds.insert(result.profile.id);
            auto existing = std::find_if(loaded.begin(), loaded.end(),
                [&result](const auto &profile) { return profile.id == result.profile.id; });
            if (existing == loaded.end()) loaded.append(result.profile);
            else *existing = result.profile;
        }
    }
    if (loaded.isEmpty()) {
        if (error) *error = QStringLiteral("no profile JSON files found in catalog paths");
        return false;
    }

    m_profiles = loaded;
    m_currentIndex = 0;
    emit itemsChanged();
    emit currentChanged();
    return true;
}

bool ProfileCatalog::selectById(const QString &id)
{
    for (int index = 0; index < m_profiles.size(); ++index) {
        if (m_profiles.at(index).id == id) {
            return selectIndex(index);
        }
    }
    return false;
}

bool ProfileCatalog::selectIndex(int index)
{
    if (index < 0 || index >= m_profiles.size() || index == m_currentIndex) {
        return index == m_currentIndex;
    }
    m_currentIndex = index;
    emit currentChanged();
    return true;
}

} // namespace QindaQt::Profiles
