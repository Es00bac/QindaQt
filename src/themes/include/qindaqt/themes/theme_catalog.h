// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QObject>
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
