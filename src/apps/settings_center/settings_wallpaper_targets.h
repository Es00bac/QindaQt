// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_appearance/wallpaper_target_catalog.h"

#include <memory>

namespace QindaQt::DisplayClient {
class Client;
}
namespace QindaQt::Apps::SettingsAppearance {
class AppearanceSettingsModel;
}

namespace QindaQt::Apps::SettingsCenter {

// ADR-0286 composition for Appearance's per-display and per-desktop wallpaper
// choices. It lends the route a WallpaperTargetCatalog fed by the Display1
// client this process already runs for the Display route (read-only
// snapshots) and, when the shell's workspace module is part of the build, by
// a read-only workspace controller for virtual desktop names. It constructs
// the one workspace transport itself, as the composition root rule requires.
//
// AGENT-GUARD: declare it after the display client and the Appearance model
// it borrows, so it is destroyed first and takes the catalog back from the
// model before the catalog dies.
class SettingsWallpaperTargets final {
public:
    SettingsWallpaperTargets(DisplayClient::Client &displays,
                             SettingsAppearance::AppearanceSettingsModel &appearance);
    ~SettingsWallpaperTargets();

    SettingsWallpaperTargets(const SettingsWallpaperTargets &) = delete;
    SettingsWallpaperTargets &operator=(const SettingsWallpaperTargets &) = delete;

private:
    struct Desktops;

    SettingsAppearance::AppearanceSettingsModel &m_appearance;
    SettingsAppearance::WallpaperTargetCatalog m_catalog;
    // Null when the workspace module is not built (QINDAQT_BUILD_SHELL=OFF):
    // Appearance then offers per-display choices without desktops.
    std::unique_ptr<Desktops> m_desktops;
};

} // namespace QindaQt::Apps::SettingsCenter
