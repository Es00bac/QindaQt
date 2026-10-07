// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_composition.h"
#include <utility>
namespace QindaQt::Apps::FileManager {
MediaComposition composeMedia(QStringList dataRoots, FolderNavigations &navigations) {
    MediaComposition media;
    media.launcher = std::make_unique<QindaQt::RemovableMedia::DesktopMediaOwnerLauncher>(std::move(dataRoots));
    media.client = std::make_unique<QindaQt::RemovableMedia::MediaClient>(QDBusConnection::sessionBus(), *media.launcher);
    media.presenter = std::make_unique<MediaPresenter>(*media.client, navigations);
    media.client->start();
    return media;
}
}
