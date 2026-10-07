// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_client/desktop_owner_launcher.h>
#include <qindaqt/services/removable_media_client/media_client.h>
#include <memory>
struct ChooserMediaComposition final {
    std::unique_ptr<QindaQt::RemovableMedia::DesktopMediaOwnerLauncher> launcher;
    std::unique_ptr<QindaQt::RemovableMedia::MediaClient> source;
};
[[nodiscard]] ChooserMediaComposition composeChooserMedia();
