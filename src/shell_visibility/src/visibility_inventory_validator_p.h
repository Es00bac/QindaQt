// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/panel_visibility_types.h"

namespace QindaQt::ShellVisibility::Private {

[[nodiscard]] PanelVisibilityError
validateInventory(const PanelVisibilityInventory &inventory);

} // namespace QindaQt::ShellVisibility::Private
