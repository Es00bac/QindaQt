// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationoutputselector.h"

#include <QSet>

#include <utility>

namespace QindaQt::Shell {
namespace {

NotificationOutputSelection failure(NotificationOutputSelectionError error,
                                    QString message)
{
    return {{}, error, std::move(message)};
}

std::optional<QSet<QString>> outputIds(
    const QVector<ShellLayout::LogicalOutput> &outputs)
{
    QSet<QString> result;
    for (const auto &output : outputs) {
        if (output.id.trimmed().isEmpty() || result.contains(output.id)) {
            return std::nullopt;
        }
        result.insert(output.id);
    }
    return result;
}

std::optional<QSet<QString>> outputIds(
    const QVector<ShellVisibility::LogicalOutputSnapshot> &outputs)
{
    QSet<QString> result;
    for (const auto &output : outputs) {
        if (output.id.trimmed().isEmpty() || result.contains(output.id)) {
            return std::nullopt;
        }
        result.insert(output.id);
    }
    return result;
}

} // namespace

NotificationOutputSelection NotificationOutputSelector::select(
    const std::optional<CompositorOutputAuthorityFrame> &authority,
    const ShellVisibility::CompositorVisibilitySnapshot *visibility,
    const QVector<ShellLayout::LogicalOutput> &qtOutputs)
{
    if (!authority || authority->outputs.isEmpty()
        || authority->uniqueOwner.trimmed().isEmpty()
        || authority->outputGeneration == 0) {
        return failure(NotificationOutputSelectionError::MissingAuthority,
                       QStringLiteral("no authoritative compositor output order is available"));
    }
    if (visibility == nullptr || visibility->outputGeneration == 0
        || visibility->outputs.isEmpty()) {
        return failure(NotificationOutputSelectionError::MissingVisibility,
                       QStringLiteral(
                           "no coherent shell visibility output generation is available"));
    }
    if (authority->outputGeneration != visibility->outputGeneration) {
        return failure(NotificationOutputSelectionError::GenerationMismatch,
                       QStringLiteral(
                           "compositor output projections belong to different generations"));
    }

    QSet<QString> authorityIds;
    for (const auto &output : authority->outputs) {
        if (output.outputId.trimmed().isEmpty()
            || authorityIds.contains(output.outputId)) {
            return failure(NotificationOutputSelectionError::InventoryMismatch,
                           QStringLiteral("authoritative output order is invalid or ambiguous"));
        }
        authorityIds.insert(output.outputId);
    }
    const auto visibilityIds = outputIds(visibility->outputs);
    const auto qtIds = outputIds(qtOutputs);
    if (!visibilityIds || !qtIds || authorityIds != *visibilityIds
        || authorityIds != *qtIds) {
        return failure(NotificationOutputSelectionError::InventoryMismatch,
                       QStringLiteral("authoritative, visibility, and Qt output IDs differ"));
    }
    return {authority->outputs.constFirst().outputId,
            NotificationOutputSelectionError::None, {}};
}

} // namespace QindaQt::Shell
