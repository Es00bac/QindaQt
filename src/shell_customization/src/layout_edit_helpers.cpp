// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_edit_helpers_p.h"

#include "qindaqt/shell_customization/editing_commands.h"

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
    return appletIndex(panel.applets, appletId);
}

qsizetype appletIndex(const QVector<Profiles::AppletSpec> &applets,
                      const QString &appletId) noexcept
{
    for (qsizetype index = 0; index < applets.size(); ++index) {
        if (applets[index].id == appletId) {
            return index;
        }
    }
    return -1;
}

QVector<Profiles::AppletSpec> *appletOwner(Profiles::LayoutProfile &profile,
                                           const QString &ownerId) noexcept
{
    if (ownerId == DesktopAppletOwnerId) {
        return &profile.desktopApplets;
    }
    const qsizetype index = panelIndex(profile, ownerId);
    return index < 0 ? nullptr : &profile.panels[index].applets;
}

const QVector<Profiles::AppletSpec> *appletOwner(
    const Profiles::LayoutProfile &profile, const QString &ownerId) noexcept
{
    if (ownerId == DesktopAppletOwnerId) {
        return &profile.desktopApplets;
    }
    const qsizetype index = panelIndex(profile, ownerId);
    return index < 0 ? nullptr : &profile.panels[index].applets;
}

bool containsApplet(const Profiles::LayoutProfile &profile,
                    const QString &appletId) noexcept
{
    for (const Profiles::PanelSpec &panel : profile.panels) {
        if (appletIndex(panel, appletId) >= 0) {
            return true;
        }
    }
    return appletIndex(profile.desktopApplets, appletId) >= 0;
}

bool sameProfile(const Profiles::LayoutProfile &first,
                 const Profiles::LayoutProfile &second)
{
    return first.toJson() == second.toJson();
}

} // namespace QindaQt::ShellCustomization::LayoutEditHelpers
