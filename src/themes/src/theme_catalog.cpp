// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/theme_catalog.h"

#include "qindaqt/themes/theme_loader.h"

#include <QSet>

#include <algorithm>

namespace QindaQt::Themes {

ThemeCatalog::ThemeCatalog(QObject *parent)
    : QObject(parent)
{
}

QVariantList ThemeCatalog::items() const
{
    QVariantList values;
    values.reserve(m_themes.size());
    for (const auto &theme : m_themes) {
        values.append(theme.toVariantMap());
    }
    return values;
}

QVariantMap ThemeCatalog::current() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_themes.size()) {
        return {};
    }
    return m_themes.at(m_currentIndex).toVariantMap();
}

int ThemeCatalog::currentIndex() const { return m_currentIndex; }
const QVector<ThemeSpec> &ThemeCatalog::themes() const { return m_themes; }

bool ThemeCatalog::loadDirectory(const QString &path, QString *error)
{
    QVector<ThemeSpec> loaded;
    QSet<QString> ids;
    for (const auto &result : ThemeLoader::fromDirectory(path)) {
        if (!result.ok) {
            if (error != nullptr) {
                *error = result.error;
            }
            return false;
        }
        if (ids.contains(result.theme.id)) {
            if (error != nullptr) {
                *error = QStringLiteral("duplicate theme id: %1").arg(result.theme.id);
            }
            return false;
        }
        ids.insert(result.theme.id);
        loaded.append(result.theme);
    }
    if (loaded.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("no theme JSON files found in %1").arg(path);
        }
        return false;
    }

    m_themes = loaded;
    m_currentIndex = 0;
    emit itemsChanged();
    emit currentChanged();
    return true;
}

bool ThemeCatalog::loadDirectories(const QStringList &paths, QString *error,
                                   QStringList *skipped)
{
    QVector<ThemeSpec> merged;
    bool haveBase = false;
    for (const QString &path : paths) {
        const auto results = ThemeLoader::fromDirectory(path);
        if (results.isEmpty()) {
            continue;
        }
        const bool base = !haveBase;
        haveBase = true;
        QSet<QString> directoryIds;
        for (const auto &result : results) {
            if (!result.ok || directoryIds.contains(result.theme.id)) {
                const QString problem = !result.ok
                    ? result.error
                    : QStringLiteral("duplicate theme id: %1 in %2").arg(result.theme.id, path);
                if (base) {
                    if (error != nullptr) {
                        *error = problem;
                    }
                    return false;
                }
                if (skipped != nullptr) {
                    skipped->append(problem);
                }
                continue;
            }
            directoryIds.insert(result.theme.id);
            const auto existing = std::find_if(merged.begin(), merged.end(),
                [&result](const ThemeSpec &theme) { return theme.id == result.theme.id; });
            if (existing == merged.end()) {
                merged.append(result.theme);
            } else {
                *existing = result.theme;
            }
        }
    }
    if (merged.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("no theme JSON files found in the catalog directories: %1")
                         .arg(paths.join(QLatin1Char(':')));
        }
        return false;
    }

    m_themes = merged;
    m_currentIndex = 0;
    emit itemsChanged();
    emit currentChanged();
    return true;
}

bool ThemeCatalog::selectById(const QString &id)
{
    for (int index = 0; index < m_themes.size(); ++index) {
        if (m_themes.at(index).id == id) {
            return selectIndex(index);
        }
    }
    return false;
}

bool ThemeCatalog::selectIndex(int index)
{
    if (index < 0 || index >= m_themes.size() || index == m_currentIndex) {
        return index == m_currentIndex;
    }
    m_currentIndex = index;
    emit currentChanged();
    return true;
}

} // namespace QindaQt::Themes
