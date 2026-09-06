// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"
#include "shortcutnotecontroller.h"
#include "wallpapercontroller.h"

#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <algorithm>

namespace QindaQt::Shell {

void ShellRuntimeApplication::initializeWallpaper()
{
    QStringList roots;
    if (!m_dataRoots.dataHome.isEmpty()) {
        roots.append(m_dataRoots.dataHome);
    }
    roots.append(m_dataRoots.dataDirectories);
    m_wallpaper = std::make_unique<WallpaperController>(
        m_application, m_engine, *m_settingsClient, roots, this);

    // AGENT-NOTE: the shortcut note (ADR-0084) rides the wallpaper background
    // surfaces and owns its purpose-scoped Settings1 client plus its KGlobalAccel
    // registration, so it stays independent of the notification-dependent
    // members and needs no additional ShellRuntimeApplication state. It is
    // QObject-parented to the wallpaper controller, which resetRuntime()
    // destroys first.
    auto *note = ShortcutNoteController::createProduction(
        m_application, m_engine, m_wallpaper.get());
    note->setTheme(effectiveThemeMap());
    int topInset = 0;
    int rightInset = 0;
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex >= 0
        && profileIndex < m_profiles.profiles().size()) {
        for (const Profiles::PanelSpec &panel :
             m_profiles.profiles().at(profileIndex).panels) {
            if (panel.edge == Profiles::Edge::Top) {
                topInset = std::max(topInset, panel.thickness);
            } else if (panel.edge == Profiles::Edge::Right) {
                rightInset = std::max(rightInset, panel.thickness);
            }
        }
    }
    note->setPanelInsets(topInset, rightInset);
    m_wallpaper->setShortcutNote(note);
    // AGENT-NOTE: queued so this runs after ShellAppearanceBridge's direct
    // snapshot handler has re-selected m_themes; effectiveThemeMap() then
    // already reflects the newly confirmed appearance. The note is the
    // connection context so its destruction in resetRuntime() disconnects
    // this refresh before the settings client goes away.
    connect(m_settingsClient.get(),
            &Services::SettingsClient::SettingsClient::snapshotChanged, note,
            [this, note] { note->setTheme(effectiveThemeMap()); },
            Qt::QueuedConnection);
    m_wallpaper->start();
}

} // namespace QindaQt::Shell
