// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_composition.h"
#include <QStandardPaths>
ChooserMediaComposition composeChooserMedia() {
    auto roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation); roots.removeDuplicates();
    ChooserMediaComposition media;
    media.launcher = std::make_unique<QindaQt::RemovableMedia::DesktopMediaOwnerLauncher>(roots);
    media.source = std::make_unique<QindaQt::RemovableMedia::MediaClient>(QDBusConnection::sessionBus(), *media.launcher);
    media.source->start(); return media;
}
