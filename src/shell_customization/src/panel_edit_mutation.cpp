// SPDX-License-Identifier: LGPL-3.0-or-later
#include "panel_edit_mutation_p.h"

#include "layout_edit_helpers_p.h"

#include <utility>

namespace QindaQt::ShellCustomization::PanelEditMutation {
namespace {

using LayoutEditHelpers::error;
using LayoutEditHelpers::panelIndex;

std::optional<qsizetype> insertionIndex(const Profiles::LayoutProfile &profile,
                                        const std::optional<QString> &beforePanelId)
{
    if (!beforePanelId.has_value()) {
        return profile.panels.size();
    }
    const qsizetype index = panelIndex(profile, *beforePanelId);
    return index < 0 ? std::nullopt : std::optional(index);
}

bool edgeChangesOrientation(Profiles::Edge previous, Profiles::Edge next)
{
    const auto isHorizontal = [](Profiles::Edge edge) -> std::optional<bool> {
        switch (edge) {
        case Profiles::Edge::Top:
        case Profiles::Edge::Bottom:
            return true;
        case Profiles::Edge::Left:
        case Profiles::Edge::Right:
            return false;
        }
        return std::nullopt;
    };

    const auto previousHorizontal = isHorizontal(previous);
    const auto nextHorizontal = isHorizontal(next);
    return previousHorizontal.has_value() && nextHorizontal.has_value()
        && *previousHorizontal != *nextHorizontal;
}

} // namespace

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const AddPanelCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    if (panelIndex(profile, command.panel.id) >= 0) {
        return error(EditingErrorCode::DuplicatePanelId,
                     QStringLiteral("panel ID '%1' is already used").arg(command.panel.id),
                     command.panel.id);
    }
    const auto index = insertionIndex(profile, command.beforePanelId);
    if (!index.has_value()) {
        return error(EditingErrorCode::UnknownAnchorId,
                     QStringLiteral("panel anchor '%1' does not exist")
                         .arg(*command.beforePanelId),
                     *command.beforePanelId);
    }
    if (auto placementError = placementValidator.validatePanel(command.panel)) {
        return placementError;
    }

    profile.panels.insert(*index, command.panel);
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const RemovePanelCommand &command,
    const AppletPlacementValidator &)
{
    const qsizetype index = panelIndex(profile, command.panelId);
    if (index < 0) {
        return error(EditingErrorCode::UnknownPanelId,
                     QStringLiteral("panel '%1' does not exist").arg(command.panelId),
                     command.panelId);
    }
    profile.panels.removeAt(index);
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const MovePanelCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    const qsizetype sourceIndex = panelIndex(profile, command.panelId);
    if (sourceIndex < 0) {
        return error(EditingErrorCode::UnknownPanelId,
                     QStringLiteral("panel '%1' does not exist").arg(command.panelId),
                     command.panelId);
    }
    if (command.beforePanelId == command.panelId) {
        return error(EditingErrorCode::InvalidCommand,
                     QStringLiteral("a panel cannot use itself as its move anchor"),
                     command.panelId);
    }
    if (command.beforePanelId.has_value()
        && panelIndex(profile, *command.beforePanelId) < 0) {
        return error(EditingErrorCode::UnknownAnchorId,
                     QStringLiteral("panel anchor '%1' does not exist")
                         .arg(*command.beforePanelId),
                     *command.beforePanelId);
    }

    Profiles::PanelSpec moved = profile.panels[sourceIndex];
    const Profiles::Edge previousEdge = moved.edge;
    moved.output = command.outputId;
    moved.edge = command.edge;
    moved.alignment = command.alignment;
    if (edgeChangesOrientation(previousEdge, moved.edge)) {
        if (auto placementError = placementValidator.validatePanel(moved)) {
            return placementError;
        }
    }

    profile.panels.removeAt(sourceIndex);
    const auto targetIndex = insertionIndex(profile, command.beforePanelId);
    // The anchor was validated before removal and cannot disappear unless it
    // was the source itself, which is rejected above.
    if (!targetIndex.has_value()) {
        return error(EditingErrorCode::UnknownAnchorId,
                     QStringLiteral("panel anchor disappeared during the move"),
                     command.beforePanelId.value_or(QString{}));
    }
    profile.panels.insert(*targetIndex, std::move(moved));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const ConfigurePanelCommand &command,
    const AppletPlacementValidator &)
{
    const qsizetype index = panelIndex(profile, command.panelId);
    if (index < 0) {
        return error(EditingErrorCode::UnknownPanelId,
                     QStringLiteral("panel '%1' does not exist").arg(command.panelId),
                     command.panelId);
    }

    Profiles::PanelSpec &panel = profile.panels[index];
    panel.layer = command.layer;
    panel.hideMode = command.hideMode;
    panel.rows = command.rows;
    panel.thickness = command.thickness;
    panel.length = command.length;
    return std::nullopt;
}

} // namespace QindaQt::ShellCustomization::PanelEditMutation
