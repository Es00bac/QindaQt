// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_catalog.h"

#include "qindaqt/profiles/profile_loader.h"

#include <QSet>

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
    QVector<LayoutProfile> loaded;
    QSet<QString> ids;
    for (const auto &result : ProfileLoader::fromDirectory(path)) {
        if (!result.ok) {
            if (error != nullptr) {
                *error = result.error.diagnostic();
            }
            return false;
        }
        if (ids.contains(result.profile.id)) {
            if (error != nullptr) {
                *error = QStringLiteral("duplicate profile id: %1").arg(result.profile.id);
            }
            return false;
        }
        ids.insert(result.profile.id);
        loaded.append(result.profile);
    }
    if (loaded.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("no profile JSON files found in %1").arg(path);
        }
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
