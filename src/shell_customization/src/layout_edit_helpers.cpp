// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_edit_helpers_p.h"

#include <utility>

namespace QindaQt::ShellCustomization::LayoutEditHelpers {

EditingError error(EditingErrorCode code,
                   QString message,
                   QString panelId,
                   QString appletId)
{
    return {code,
            std::move(message),
            std::move(panelId),
            std::move(appletId)};
}

qsizetype panelIndex(const Profiles::LayoutProfile &profile,
                     const QString &panelId) noexcept
{
    for (qsizetype index = 0; index < profile.panels.size(); ++index) {
        if (profile.panels[index].id == panelId) {
            return index;
        }
    }
    return -1;
}

qsizetype appletIndex(const Profiles::PanelSpec &panel,
                      const QString &appletId) noexcept
{
    for (qsizetype index = 0; index < panel.applets.size(); ++index) {
        if (panel.applets[index].id == appletId) {
            return index;
        }
    }
    return -1;
}

bool containsApplet(const Profiles::LayoutProfile &profile,
                    const QString &appletId) noexcept
{
    for (const Profiles::PanelSpec &panel : profile.panels) {
        if (appletIndex(panel, appletId) >= 0) {
            return true;
        }
    }
    return false;
}

} // namespace QindaQt::ShellCustomization::LayoutEditHelpers
