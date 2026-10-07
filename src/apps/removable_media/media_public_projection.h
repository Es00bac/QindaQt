// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_types.h"
#include <qindaqt/services/removable_media_protocol/media_types.h>

namespace QindaQt::Apps::RemovableMedia {
// Pure adapter over complete owning-backend facts. Public ids are salted with
// instance epoch and contain no private path/choice key. No inventory policy,
// UDisks calls, mount action or filesystem probing occurs in this projection.
[[nodiscard]] QindaQt::RemovableMedia::VolumeRow publicVolume(
    const Volume &volume, const QString &epoch, bool busy);
}
