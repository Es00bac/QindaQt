// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_orchestration/panel_visibility_inventory_assembler.h"

#include "qindaqt/profiles/profile_validation.h"
#include "qindaqt/shell_orchestration/output_inventory_matcher.h"
#include "qindaqt/shell_visibility/panel_visibility_policy.h"

#include <QHash>
#include <QSet>

#include <utility>

namespace QindaQt::ShellOrchestration {
namespace {

using ErrorCode = PanelVisibilityAssemblyErrorCode;
using Identity = ShellVisibility::PanelSurfaceIdentity;
using Result = PanelVisibilityAssemblyResult;

Result failure(ErrorCode code, QString message, Identity identity = {},
               ShellVisibility::PanelVisibilityError inventoryError = {})
{
    return {{}, {}, {code, std::move(identity), std::move(message),
                     std::move(inventoryError)}};
}

bool layerReservesWorkArea(Profiles::Layer layer)
{
    return layer == Profiles::Layer::Normal || layer == Profiles::Layer::Above;
}

QHash<QString, const Profiles::PanelSpec *> profilePanels(
    const Profiles::LayoutProfile &profile)
{
    QHash<QString, const Profiles::PanelSpec *> result;
    for (const auto &panel : profile.panels) {
        result.insert(panel.id, &panel);
    }
    return result;
}

} // namespace

PanelVisibilityAssemblyResult PanelVisibilityInventoryAssembler::assemble(
    const Profiles::LayoutProfile &profile,
    const ShellLayout::PanelLayoutResult &layout,
    const ShellVisibility::CompositorVisibilitySnapshot &compositor,
    const QVector<ShellVisibility::PanelInteractionSnapshot> &interactions)
{
    const auto profileValidation = Profiles::ProfileValidator::validate(profile);
    if (!profileValidation.succeeded()) {
        return failure(ErrorCode::InvalidProfile,
                       profileValidation.error.diagnostic());
    }
    if (!layout.ok()) {
        return failure(ErrorCode::RejectedLayout,
                       QStringLiteral("cannot assemble visibility from a rejected layout: %1")
                           .arg(layout.error.message));
    }
    if (compositor.epoch.trimmed().isEmpty() || compositor.revision == 0) {
        return failure(ErrorCode::InvalidInventory,
                       QStringLiteral("compositor lineage is not a valid published generation"));
    }
    QVector<ShellLayout::LogicalOutput> compositorOutputs;
    compositorOutputs.reserve(compositor.outputs.size());
    for (const auto &output : compositor.outputs) {
        compositorOutputs.append({output.id, output.geometry, output.scale});
    }
    QVector<ShellLayout::LogicalOutput> solvedOutputs;
    solvedOutputs.reserve(layout.outputs.size());
    QSet<QString> solvedOutputIds;
    for (const auto &output : layout.outputs) {
        solvedOutputs.append({output.outputId, output.geometry, output.scale});
        solvedOutputIds.insert(output.outputId);
    }
    const auto outputMatch = OutputInventoryMatcher::match(compositorOutputs,
                                                            solvedOutputs);
    if (!outputMatch.ok()) {
        return failure(ErrorCode::OutputMismatch, outputMatch.message,
                       {{}, outputMatch.outputId});
    }

    const auto panelsById = profilePanels(profile);
    QHash<QString, QSet<QString>> expected;
    for (const auto &panel : profile.panels) {
        if (panel.output == QStringLiteral("*")) {
            for (const auto &outputId : solvedOutputIds) {
                expected[outputId].insert(panel.id);
            }
        } else if (!solvedOutputIds.contains(panel.output)) {
            return failure(ErrorCode::MissingSurface,
                           QStringLiteral("profile panel '%1' targets missing output '%2'")
                               .arg(panel.id, panel.output),
                           {panel.id, panel.output});
        } else {
            expected[panel.output].insert(panel.id);
        }
    }

    ShellVisibility::PanelVisibilityInventory inventory;
    inventory.outputs = compositor.outputs;
    inventory.windows = compositor.windows;
    inventory.scope = compositor.scope;
    inventory.interactions = interactions;
    inventory.panels.reserve(layout.surfaces.size());
    QHash<QString, QSet<QString>> actual;
    for (const auto &surface : layout.surfaces) {
        const Identity identity{surface.panelId, surface.outputId};
        auto &outputPanels = actual[surface.outputId];
        if (outputPanels.contains(surface.panelId)) {
            return failure(ErrorCode::DuplicateSurface,
                           QStringLiteral("solved panel '%1' on '%2' is duplicated")
                               .arg(surface.panelId, surface.outputId),
                           identity);
        }
        outputPanels.insert(surface.panelId);
        const auto panel = panelsById.constFind(surface.panelId);
        if (panel == panelsById.cend()) {
            return failure(ErrorCode::UnknownPanel,
                           QStringLiteral("solved panel '%1' is absent from the profile")
                               .arg(surface.panelId),
                           identity);
        }
        const bool targetMatches = panel.value()->output == QStringLiteral("*") ||
            panel.value()->output == surface.outputId;
        if (!targetMatches || panel.value()->edge != surface.edge ||
            panel.value()->layer != surface.layer ||
            surface.reservesWorkArea != layerReservesWorkArea(panel.value()->layer)) {
            return failure(ErrorCode::SurfaceContractMismatch,
                           QStringLiteral("solved panel '%1' changed its profile contract")
                               .arg(surface.panelId),
                           identity);
        }
        inventory.panels.append({
            identity,
            surface.geometry,
            panel.value()->hideMode,
            surface.reservesWorkArea
                ? ShellVisibility::PanelReservationPolicy::ReserveWhenVisible
                : ShellVisibility::PanelReservationPolicy::NeverReserve,
        });
    }

    if (actual != expected) {
        for (auto output = expected.cbegin(); output != expected.cend(); ++output) {
            for (const auto &panelId : output.value()) {
                if (!actual.value(output.key()).contains(panelId)) {
                    return failure(ErrorCode::MissingSurface,
                                   QStringLiteral("profile panel '%1' on '%2' has no solved surface")
                                       .arg(panelId, output.key()),
                                   {panelId, output.key()});
                }
            }
        }
        return failure(ErrorCode::UnknownPanel,
                       QStringLiteral("solved layout contains an unexpected panel identity"));
    }

    auto evaluation = ShellVisibility::PanelVisibilityPolicy::evaluate(inventory);
    if (!evaluation.ok()) {
        const auto inventoryError = evaluation.error;
        return failure(ErrorCode::InvalidInventory,
                       QStringLiteral("assembled visibility inventory is invalid: %1")
                           .arg(inventoryError.message),
                       inventoryError.panel, inventoryError);
    }
    return {std::move(inventory), std::move(evaluation), {}};
}

} // namespace QindaQt::ShellOrchestration
