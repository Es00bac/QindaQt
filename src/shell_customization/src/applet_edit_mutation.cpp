// SPDX-License-Identifier: LGPL-3.0-or-later
#include "applet_edit_mutation_p.h"

#include "layout_edit_helpers_p.h"

#include <utility>

namespace QindaQt::ShellCustomization::AppletEditMutation {
namespace {

using LayoutEditHelpers::appletIndex;
using LayoutEditHelpers::containsApplet;
using LayoutEditHelpers::error;
using LayoutEditHelpers::panelIndex;

std::optional<qsizetype> insertionIndex(const Profiles::PanelSpec &panel,
                                        const std::optional<QString> &beforeAppletId)
{
    if (!beforeAppletId.has_value()) {
        return panel.applets.size();
    }
    const qsizetype index = appletIndex(panel, *beforeAppletId);
    return index < 0 ? std::nullopt : std::optional(index);
}

std::optional<EditingError> unknownPanel(const QString &role,
                                         const QString &panelId)
{
    return error(EditingErrorCode::UnknownPanelId,
                 QStringLiteral("%1 panel '%2' does not exist").arg(role, panelId),
                 panelId);
}

std::optional<EditingError> unknownApplet(const QString &panelId,
                                          const QString &appletId)
{
    return error(EditingErrorCode::UnknownAppletId,
                 QStringLiteral("applet '%1' does not exist in panel '%2'")
                     .arg(appletId, panelId),
                 panelId,
                 appletId);
}

std::optional<EditingError> unknownAnchor(const QString &panelId,
                                          const QString &anchorId)
{
    return error(EditingErrorCode::UnknownAnchorId,
                 QStringLiteral("applet anchor '%1' does not exist in panel '%2'")
                     .arg(anchorId, panelId),
                 panelId,
                 anchorId);
}

} // namespace

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const InsertAppletCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    const qsizetype targetIndex = panelIndex(profile, command.panelId);
    if (targetIndex < 0) {
        return unknownPanel(QStringLiteral("target"), command.panelId);
    }
    if (command.instanceId.trimmed().isEmpty()) {
        return error(EditingErrorCode::InvalidCommand,
                     QStringLiteral("applet instance ID must not be blank"),
                     command.panelId);
    }
    if (containsApplet(profile, command.instanceId)) {
        return error(EditingErrorCode::DuplicateAppletId,
                     QStringLiteral("applet instance ID '%1' is already used")
                         .arg(command.instanceId),
                     command.panelId,
                     command.instanceId);
    }

    Profiles::PanelSpec &target = profile.panels[targetIndex];
    const auto insertion = insertionIndex(target, command.beforeAppletId);
    if (!insertion.has_value()) {
        return unknownAnchor(command.panelId, *command.beforeAppletId);
    }
    Profiles::AppletSpec inserted{
        .id = command.instanceId,
        .plugin = command.pluginId,
        .settings = command.initialSettings,
    };
    if (auto placementError = placementValidator.validatePlacement(inserted, target)) {
        return placementError;
    }
    target.applets.insert(*insertion, std::move(inserted));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const MoveAppletCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    const qsizetype sourcePanelIndex = panelIndex(profile, command.sourcePanelId);
    const qsizetype targetPanelIndex = panelIndex(profile, command.targetPanelId);
    if (sourcePanelIndex < 0) {
        return unknownPanel(QStringLiteral("source"), command.sourcePanelId);
    }
    if (targetPanelIndex < 0) {
        return unknownPanel(QStringLiteral("target"), command.targetPanelId);
    }

    const qsizetype sourceAppletIndex =
        appletIndex(profile.panels[sourcePanelIndex], command.appletId);
    if (sourceAppletIndex < 0) {
        return unknownApplet(command.sourcePanelId, command.appletId);
    }
    if (command.beforeAppletId == command.appletId) {
        return error(EditingErrorCode::InvalidCommand,
                     QStringLiteral("an applet cannot use itself as its move anchor"),
                     command.targetPanelId,
                     command.appletId);
    }
    if (command.beforeAppletId.has_value()
        && appletIndex(profile.panels[targetPanelIndex], *command.beforeAppletId) < 0) {
        return unknownAnchor(command.targetPanelId, *command.beforeAppletId);
    }

    Profiles::AppletSpec moved =
        profile.panels[sourcePanelIndex].applets[sourceAppletIndex];
    if (AppletPlacementValidator::placementChanges(
            moved,
            profile.panels[sourcePanelIndex],
            profile.panels[targetPanelIndex])) {
        if (auto placementError = placementValidator.validatePlacement(
                moved, profile.panels[targetPanelIndex])) {
            return placementError;
        }
    }

    profile.panels[sourcePanelIndex].applets.removeAt(sourceAppletIndex);
    Profiles::PanelSpec &target = profile.panels[targetPanelIndex];
    const auto insertion = insertionIndex(target, command.beforeAppletId);
    if (!insertion.has_value()) {
        return unknownAnchor(command.targetPanelId,
                             command.beforeAppletId.value_or(QString{}));
    }
    target.applets.insert(*insertion, std::move(moved));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const RemoveAppletCommand &command,
    const AppletPlacementValidator &)
{
    const qsizetype ownerIndex = panelIndex(profile, command.panelId);
    if (ownerIndex < 0) {
        return unknownPanel(QStringLiteral("owner"), command.panelId);
    }
    Profiles::PanelSpec &owner = profile.panels[ownerIndex];
    const qsizetype index = appletIndex(owner, command.appletId);
    if (index < 0) {
        return unknownApplet(command.panelId, command.appletId);
    }
    owner.applets.removeAt(index);
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const DuplicateAppletCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    const qsizetype sourcePanelIndex = panelIndex(profile, command.sourcePanelId);
    const qsizetype targetPanelIndex = panelIndex(profile, command.targetPanelId);
    if (sourcePanelIndex < 0) {
        return unknownPanel(QStringLiteral("source"), command.sourcePanelId);
    }
    if (targetPanelIndex < 0) {
        return unknownPanel(QStringLiteral("target"), command.targetPanelId);
    }
    const qsizetype sourceAppletIndex =
        appletIndex(profile.panels[sourcePanelIndex], command.appletId);
    if (sourceAppletIndex < 0) {
        return unknownApplet(command.sourcePanelId, command.appletId);
    }
    if (command.newAppletId.trimmed().isEmpty()) {
        return error(EditingErrorCode::InvalidCommand,
                     QStringLiteral("duplicated applet instance ID must not be blank"),
                     command.targetPanelId);
    }
    if (containsApplet(profile, command.newAppletId)) {
        return error(EditingErrorCode::DuplicateAppletId,
                     QStringLiteral("applet instance ID '%1' is already used")
                         .arg(command.newAppletId),
                     command.targetPanelId,
                     command.newAppletId);
    }

    Profiles::PanelSpec &target = profile.panels[targetPanelIndex];
    const auto insertion = insertionIndex(target, command.beforeAppletId);
    if (!insertion.has_value()) {
        return unknownAnchor(command.targetPanelId, *command.beforeAppletId);
    }
    Profiles::AppletSpec duplicate =
        profile.panels[sourcePanelIndex].applets[sourceAppletIndex];
    duplicate.id = command.newAppletId;
    if (auto placementError = placementValidator.validatePlacement(duplicate, target)) {
        return placementError;
    }
    target.applets.insert(*insertion, std::move(duplicate));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const UpdateAppletSettingsCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    const qsizetype ownerIndex = panelIndex(profile, command.panelId);
    if (ownerIndex < 0) {
        return unknownPanel(QStringLiteral("owner"), command.panelId);
    }
    Profiles::PanelSpec &owner = profile.panels[ownerIndex];
    const qsizetype index = appletIndex(owner, command.appletId);
    if (index < 0) {
        return unknownApplet(command.panelId, command.appletId);
    }

    Profiles::AppletSpec updated = owner.applets[index];
    updated.settings = command.settings;
    if (AppletPlacementValidator::zonePlacementChanges(
            owner.applets[index].settings, command.settings)) {
        if (auto placementError = placementValidator.validatePlacement(updated, owner)) {
            return placementError;
        }
    }
    owner.applets[index] = std::move(updated);
    return std::nullopt;
}

} // namespace QindaQt::ShellCustomization::AppletEditMutation
