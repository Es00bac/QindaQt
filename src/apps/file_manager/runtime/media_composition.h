// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_presenter.h"
#include <qindaqt/services/removable_media_client/desktop_owner_launcher.h>
#include <qindaqt/services/removable_media_client/media_client.h>
#include <memory>
namespace QindaQt::Apps::FileManager {
// Declaration order makes the GUI presenter die before client, then launcher.
struct MediaComposition final {
    std::unique_ptr<QindaQt::RemovableMedia::DesktopMediaOwnerLauncher> launcher;
    std::unique_ptr<QindaQt::RemovableMedia::MediaClient> client;
    std::unique_ptr<MediaPresenter> presenter;
};
[[nodiscard]] MediaComposition composeMedia(QStringList dataRoots, FolderNavigations &);
}
