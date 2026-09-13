// SPDX-License-Identifier: LGPL-3.0-or-later
#include "applet_edit_mutation_p.h"

#include "layout_edit_helpers_p.h"

#include <utility>

namespace QindaQt::ShellCustomization::AppletEditMutation {
namespace {

using LayoutEditHelpers::appletIndex;
using LayoutEditHelpers::appletOwner;
using LayoutEditHelpers::containsApplet;
using LayoutEditHelpers::error;
using LayoutEditHelpers::panelIndex;

std::optional<qsizetype> insertionIndex(const QVector<Profiles::AppletSpec> &applets,
                                        const std::optional<QString> &beforeAppletId)
{
    if (!beforeAppletId.has_value()) {
        return applets.size();
    }
    const qsizetype index = appletIndex(applets, *beforeAppletId);
    return index < 0 ? std::nullopt : std::optional(index);
}

std::optional<EditingError> unknownOwner(const QString &role,
                                         const QString &ownerId)
{
    return error(EditingErrorCode::UnknownPanelId,
                 QStringLiteral("%1 applet owner '%2' does not exist").arg(role, ownerId),
                 ownerId);
}

std::optional<EditingError> validateTarget(
    const Profiles::LayoutProfile &profile,
    const QString &ownerId,
    const Profiles::AppletSpec &applet,
    const AppletPlacementValidator &placementValidator)
{
    if (ownerId == DesktopAppletOwnerId) {
        return placementValidator.validateDesktopPlacement(applet);
    }
    const qsizetype index = panelIndex(profile, ownerId);
    if (index < 0) {
        return unknownOwner(QStringLiteral("target"), ownerId);
    }
    return placementValidator.validatePlacement(applet, profile.panels[index]);
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
    QVector<Profiles::AppletSpec> *target = appletOwner(profile, command.panelId);
    if (target == nullptr) {
        return unknownOwner(QStringLiteral("target"), command.panelId);
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

    const auto insertion = insertionIndex(*target, command.beforeAppletId);
    if (!insertion.has_value()) {
        return unknownAnchor(command.panelId, *command.beforeAppletId);
    }
    Profiles::AppletSpec inserted{
        .id = command.instanceId,
        .plugin = command.pluginId,
        .settings = command.initialSettings,
    };
    if (auto placementError = validateTarget(profile, command.panelId, inserted,
                                              placementValidator)) {
        return placementError;
    }
    target->insert(*insertion, std::move(inserted));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const MoveAppletCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    QVector<Profiles::AppletSpec> *source = appletOwner(profile, command.sourcePanelId);
    QVector<Profiles::AppletSpec> *target = appletOwner(profile, command.targetPanelId);
    if (source == nullptr) {
        return unknownOwner(QStringLiteral("source"), command.sourcePanelId);
    }
    if (target == nullptr) {
        return unknownOwner(QStringLiteral("target"), command.targetPanelId);
    }

    const qsizetype sourceAppletIndex = appletIndex(*source, command.appletId);
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
        && appletIndex(*target, *command.beforeAppletId) < 0) {
        return unknownAnchor(command.targetPanelId, *command.beforeAppletId);
    }

    Profiles::AppletSpec moved = source->at(sourceAppletIndex);
    bool placementChanged = command.sourcePanelId != command.targetPanelId;
    if (placementChanged
        && command.sourcePanelId != DesktopAppletOwnerId
        && command.targetPanelId != DesktopAppletOwnerId) {
        const auto sourcePanelIndex = panelIndex(profile, command.sourcePanelId);
        const auto targetPanelIndex = panelIndex(profile, command.targetPanelId);
        placementChanged = AppletPlacementValidator::placementChanges(
            moved, profile.panels.at(sourcePanelIndex), profile.panels.at(targetPanelIndex));
    }
    if (placementChanged) {
        if (auto placementError = validateTarget(profile, command.targetPanelId,
                                                  moved, placementValidator)) {
            return placementError;
        }
    }

    source->removeAt(sourceAppletIndex);
    // Removing from the same vector can invalidate the anchor's prior index,
    // so resolve the insertion only after removal.
    const auto insertion = insertionIndex(*target, command.beforeAppletId);
    if (!insertion.has_value()) {
        return unknownAnchor(command.targetPanelId,
                             command.beforeAppletId.value_or(QString{}));
    }
    target->insert(*insertion, std::move(moved));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const RemoveAppletCommand &command,
    const AppletPlacementValidator &)
{
    QVector<Profiles::AppletSpec> *owner = appletOwner(profile, command.panelId);
    if (owner == nullptr) {
        return unknownOwner(QStringLiteral("owner"), command.panelId);
    }
    const qsizetype index = appletIndex(*owner, command.appletId);
    if (index < 0) {
        return unknownApplet(command.panelId, command.appletId);
    }
    owner->removeAt(index);
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const DuplicateAppletCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    const QVector<Profiles::AppletSpec> *source = appletOwner(
        std::as_const(profile), command.sourcePanelId);
    QVector<Profiles::AppletSpec> *target = appletOwner(profile, command.targetPanelId);
    if (source == nullptr) {
        return unknownOwner(QStringLiteral("source"), command.sourcePanelId);
    }
    if (target == nullptr) {
        return unknownOwner(QStringLiteral("target"), command.targetPanelId);
    }
    const qsizetype sourceAppletIndex = appletIndex(*source, command.appletId);
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

    const auto insertion = insertionIndex(*target, command.beforeAppletId);
    if (!insertion.has_value()) {
        return unknownAnchor(command.targetPanelId, *command.beforeAppletId);
    }
    Profiles::AppletSpec duplicate = source->at(sourceAppletIndex);
    duplicate.id = command.newAppletId;
    if (auto placementError = validateTarget(profile, command.targetPanelId,
                                              duplicate, placementValidator)) {
        return placementError;
    }
    target->insert(*insertion, std::move(duplicate));
    return std::nullopt;
}

std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const UpdateAppletSettingsCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    QVector<Profiles::AppletSpec> *owner = appletOwner(profile, command.panelId);
    if (owner == nullptr) {
        return unknownOwner(QStringLiteral("owner"), command.panelId);
    }
    const qsizetype index = appletIndex(*owner, command.appletId);
    if (index < 0) {
        return unknownApplet(command.panelId, command.appletId);
    }

    Profiles::AppletSpec updated = owner->at(index);
    updated.settings = command.settings;
    if (command.panelId != DesktopAppletOwnerId
        && AppletPlacementValidator::zonePlacementChanges(
            owner->at(index).settings, command.settings)) {
        if (auto placementError = validateTarget(profile, command.panelId,
                                                  updated, placementValidator)) {
            return placementError;
        }
    }
    owner->operator[](index) = std::move(updated);
    return std::nullopt;
}

} // namespace QindaQt::ShellCustomization::AppletEditMutation
