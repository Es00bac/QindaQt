// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/game.h"

#include <QImage>

namespace QindaQt::QindaLutris {

// Cover/icon resolution for the gameicon image provider, split out of the
// controller (which only finds the Game). The cover file first, decoded
// with a 64 MiB allocation limit; else the themed icon; else fixed hicolor
// paths by name. Null when nothing exists. GUI thread (QIcon).
[[nodiscard]] QImage resolveGameImage(const Game &game);

} // namespace QindaQt::QindaLutris
