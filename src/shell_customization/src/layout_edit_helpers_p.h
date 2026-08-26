// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_result.h"

#include <QString>

namespace QindaQt::ShellCustomization::LayoutEditHelpers {

[[nodiscard]] EditingError error(EditingErrorCode code,
                                 QString message,
                                 QString panelId = {},
                                 QString appletId = {});
[[nodiscard]] qsizetype panelIndex(const Profiles::LayoutProfile &profile,
                                   const QString &panelId) noexcept;
[[nodiscard]] qsizetype appletIndex(const Profiles::PanelSpec &panel,
                                    const QString &appletId) noexcept;
[[nodiscard]] bool containsApplet(const Profiles::LayoutProfile &profile,
                                  const QString &appletId) noexcept;

} // namespace QindaQt::ShellCustomization::LayoutEditHelpers
