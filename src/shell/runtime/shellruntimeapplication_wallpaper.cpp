// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"
#include "wallpapercontroller.h"

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
    m_wallpaper->start();
}

} // namespace QindaQt::Shell
