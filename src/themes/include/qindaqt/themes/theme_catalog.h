// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace QindaQt::Themes {

class ThemeCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)

public:
    explicit ThemeCatalog(QObject *parent = nullptr);

    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QVariantMap current() const;
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] const QVector<ThemeSpec> &themes() const;

    bool loadDirectory(const QString &path, QString *error = nullptr);
    // Merges every directory, LOW to HIGH precedence: a theme in a later
    // directory replaces the earlier theme with the same id, in place, and a
    // new id is appended. The first directory that holds any theme file is
    // the base layer and is strict, like loadDirectory: an invalid or
    // duplicate file there fails the load. Every later directory is an
    // override layer, where an invalid or duplicate file is skipped and
    // described in `skipped` instead, so a stale or hand-edited user theme can
    // neither hide the installed set nor stop the shell from starting. Fails
    // only when no theme loads at all. The catalog is replaced atomically and
    // selects the first theme, like loadDirectory.
    bool loadDirectories(const QStringList &paths, QString *error = nullptr,
                         QStringList *skipped = nullptr);
    Q_INVOKABLE bool selectById(const QString &id);
    Q_INVOKABLE bool selectIndex(int index);

signals:
    void itemsChanged();
    void currentChanged();

private:
    QVector<ThemeSpec> m_themes;
    int m_currentIndex = -1;
};

} // namespace QindaQt::Themes
