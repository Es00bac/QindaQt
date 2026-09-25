// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/proton_pin.h"

#include <QVariantList>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the rows LibraryController::protonChoices() hands QML
// (AddWineDialog), one per catalog build, the default build FIRST:
//   {name: unique display label, path: absolute build dir, build: dir name,
//    version: version label, origin: "system"|"user"|"steam", removable,
//    pinnable, status: "" or e.g. "Updated by Steam — not pinnable",
//    isDefault}
// Labels are disambiguated when display names collide: "<display>
// (<dir name>, <origin>)", then the path if that still collides. The dialog
// hands `path` back to addWineGame; non-pinnable rows are for display only.
[[nodiscard]] QVariantList protonChoicesFor(
    const QVector<ProtonBuild> &builds, const std::optional<ProtonBuild> &defaultBuild);

} // namespace QindaQt::QindaLutris
