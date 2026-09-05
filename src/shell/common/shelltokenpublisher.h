// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

class QQmlEngine;

namespace QindaQt::DesignTokens {
class TokenFacade;
}

namespace QindaQt::Themes {
class ThemeCatalog;
}

namespace QindaQt::Shell {

// Publishes the shell-selected theme into one engine-owned QST facade. The
// caller owns startup and fail-closed process policy; this object owns only the
// theme-to-engine composition boundary.
class ShellTokenPublisher final : public QObject {
    Q_OBJECT

public:
    ShellTokenPublisher(QQmlEngine &engine, Themes::ThemeCatalog &themes,
                        QObject *parent = nullptr);

    [[nodiscard]] bool start(QString *error = nullptr);
    [[nodiscard]] DesignTokens::TokenFacade *facade() const;

signals:
    void publicationFailed(const QString &error);

private:
    [[nodiscard]] bool publishSelected(QString *error);

    QQmlEngine &m_engine;
    Themes::ThemeCatalog &m_themes;
    DesignTokens::TokenFacade *m_facade = nullptr;
};

} // namespace QindaQt::Shell
