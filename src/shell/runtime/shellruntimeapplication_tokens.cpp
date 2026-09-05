// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "../common/shelltokenpublisher.h"
#include "../common/shelliconconfiguration.h"
#include "shellappearancebridge.h"

#include "qindaqt/shell/icons/icon_runtime.h"

#include <QCoreApplication>
#include <QDebug>

namespace QindaQt::Shell {

bool ShellRuntimeApplication::initializeTokens(QString *error)
{
    m_tokenPublisher =
        std::make_unique<ShellTokenPublisher>(m_engine, m_themes, this);
    // Confirmed startup accessibility preferences must be part of the very
    // first publication, before any panel or hosted-applet QML exists.
    if (m_startupPreferences.has_value()) {
        m_tokenPublisher->setAccessibilityInputs(
            m_startupPreferences->accessibility);
        m_tokenPublisher->setFontFamilyOverride(m_startupPreferences->fontFamily);
    }
    connect(m_tokenPublisher.get(), &ShellTokenPublisher::publicationFailed,
            this, [](const QString &message) {
                qCritical().noquote()
                    << "QindaQt shell token publication failed:" << message;
                QCoreApplication::exit(4);
            });
    if (m_tokenPublisher->start(error)) {
        return true;
    }
    if (error != nullptr) {
        *error = QStringLiteral("QindaQt shell token publication failed: %1")
                     .arg(*error);
    }
    return false;
}

bool ShellRuntimeApplication::initializeIcons(QString *error)
{
    QString themeName;
    if (!ShellIconConfiguration::selectedThemeName(m_themes, &themeName, error)) {
        return false;
    }
    const QStringList iconRoots = Icons::IconRuntime::freedesktopIconRoots(
        m_dataRoots.dataHome, m_dataRoots.dataDirectories);
    if (!Icons::IconRuntime::install(m_engine, iconRoots, {themeName})) {
        if (error != nullptr) {
            *error = QStringLiteral("QindaQt shell icon runtime was already installed");
        }
        return false;
    }
    return true;
}

void ShellRuntimeApplication::initializeAppearanceBridge(
    bool explicitThemeSelection)
{
    m_appearanceBridge = std::make_unique<ShellAppearanceBridge>(
        *m_settingsClient, m_themes, *m_tokenPublisher, explicitThemeSelection);
    // Confirmed theme/font preference changes reach both token publication
    // (bridge/publisher) and the raw theme maps of existing and future panel
    // and notification surfaces.
    connect(&m_themes, &Themes::ThemeCatalog::currentChanged, this,
            &ShellRuntimeApplication::propagateThemeToSurfaces);
    connect(m_appearanceBridge.get(),
            &ShellAppearanceBridge::confirmedPreferencesChanged, this,
            &ShellRuntimeApplication::propagateThemeToSurfaces);
}

} // namespace QindaQt::Shell
