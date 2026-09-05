// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

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

    // Confirmed accessibility preferences for every subsequent publication,
    // including theme changes. A changed set republishes immediately with the
    // currently selected theme; safe to call before start().
    void setAccessibilityInputs(
        const DesignTokens::AccessibilityInputs &inputs);
    [[nodiscard]] DesignTokens::AccessibilityInputs accessibilityInputs() const
    {
        return m_accessibilityInputs;
    }

    // Confirmed fonts.family preference overlaid onto the selected theme for
    // every publication. A changed family republishes immediately; safe to
    // call before start().
    void setFontFamilyOverride(const QString &fontFamily);
    [[nodiscard]] QString fontFamilyOverride() const
    {
        return m_fontFamilyOverride;
    }

signals:
    void publicationFailed(const QString &error);

private:
    [[nodiscard]] bool publishSelected(QString *error);

    QQmlEngine &m_engine;
    Themes::ThemeCatalog &m_themes;
    DesignTokens::AccessibilityInputs m_accessibilityInputs;
    QString m_fontFamilyOverride;
    DesignTokens::TokenFacade *m_facade = nullptr;
};

} // namespace QindaQt::Shell
