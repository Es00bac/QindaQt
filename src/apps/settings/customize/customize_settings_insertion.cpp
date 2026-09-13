// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_settings_model.h"

#include "qindaqt/shell_customization/editing_commands.h"

namespace QindaQt::Apps::SettingsCustomize {

bool CustomizeSettingsModel::keyboardInsert(const QString &pluginId,
                                             const QString &panelId,
                                             const QString &zone,
                                             const QString &beforeAppletId)
{
    return startPaletteDrag(pluginId)
        && hoverDropTarget(panelId, zone, beforeAppletId)
        && commitDrag();
}

bool CustomizeSettingsModel::keyboardInsertDefault(const QString &pluginId)
{
    const auto *manifest = findManifest(pluginId);
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    if (manifest == nullptr || profile == nullptr) {
        return false;
    }
    const auto supports = [manifest](Applets::PlacementZone zone) {
        return manifest->placementZones.contains(zone);
    };
    const bool supportsPanel = supports(Applets::PlacementZone::PanelStart)
        || supports(Applets::PlacementZone::PanelCenter)
        || supports(Applets::PlacementZone::PanelEnd);
    if (!supportsPanel && supports(Applets::PlacementZone::Desktop)) {
        return keyboardInsert(pluginId, ShellCustomization::DesktopAppletOwnerId,
                              QStringLiteral("desktop"), {});
    }
    for (const auto &panel : profile->panels) {
        const bool horizontal = panel.edge == Profiles::Edge::Top
            || panel.edge == Profiles::Edge::Bottom;
        const auto orientation = horizontal ? Applets::Orientation::Horizontal
                                            : Applets::Orientation::Vertical;
        if (!manifest->orientations.contains(orientation)) {
            continue;
        }
        if (supports(Applets::PlacementZone::PanelStart)) {
            return keyboardInsert(pluginId, panel.id, QStringLiteral("start"), {});
        }
        if (supports(Applets::PlacementZone::PanelCenter)) {
            return keyboardInsert(pluginId, panel.id, QStringLiteral("center"), {});
        }
        if (supports(Applets::PlacementZone::PanelEnd)) {
            return keyboardInsert(pluginId, panel.id, QStringLiteral("end"), {});
        }
    }
    if (supports(Applets::PlacementZone::Desktop)) {
        return keyboardInsert(pluginId, ShellCustomization::DesktopAppletOwnerId,
                              QStringLiteral("desktop"), {});
    }
    return false;
}

} // namespace QindaQt::Apps::SettingsCustomize
