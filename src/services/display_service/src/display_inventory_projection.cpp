// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_service/display_inventory.h>

#include <qindaqt/services/display_identity/identity_resolver.h>
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_topology/topology.h>

#include "display_inventory_validation_p.h"

#include <cmath>
#include <utility>

namespace QindaQt::DisplayService
{
namespace
{

InventoryProjectionResult failure(InventoryError error, QString reason)
{
    return {.snapshot = {}, .error = error, .reasonCode = std::move(reason)};
}

QSize currentPixelSize(const InventoryOutput &output)
{
    const double transformedWidth = output.geometry.width() * output.scale;
    const double transformedHeight = output.geometry.height() * output.scale;
    if (!std::isfinite(transformedWidth) || !std::isfinite(transformedHeight)
        || transformedWidth > Display::kMaxPixelDimension + 0.5
        || transformedHeight > Display::kMaxPixelDimension + 0.5) {
        return {};
    }
    QSize transformed(static_cast<int>(std::floor(transformedWidth + 0.5)),
                      static_cast<int>(std::floor(transformedHeight + 0.5)));
    if (DisplayTopology::transposesDimensions(output.transform)) {
        transformed.transpose();
    }
    return transformed;
}

QString currentModeId(const QSize &pixelSize, quint32 refreshMilliHertz)
{
    return QStringLiteral("current:%1x%2@%3")
        .arg(pixelSize.width())
        .arg(pixelSize.height())
        .arg(refreshMilliHertz);
}

} // namespace

InventoryProjectionResult projectInventory(const InventoryFrame &frame,
                                            const QString &serviceEpoch)
{
    if (!Private::validUniqueBusOwner(frame.uniqueOwner)) {
        return failure(InventoryError::InvalidOwner, QStringLiteral("invalid-source-owner"));
    }
    if (frame.outputGeneration == 0) {
        return failure(InventoryError::InvalidGeneration,
                       QStringLiteral("invalid-output-generation"));
    }
    if (frame.outputs.isEmpty() || frame.outputs.size() > Display::kMaxOutputs) {
        return failure(InventoryError::InvalidOutput,
                       QStringLiteral("invalid-output-count"));
    }

    QList<DisplayIdentity::ObservedOutput> identityInputs;
    identityInputs.reserve(frame.outputs.size());
    for (const InventoryOutput &output : frame.outputs) {
        identityInputs.push_back({.connectorName = output.name,
                                  .runtimeCompositorUuid =
                                      output.runtimeCompositorUuid,
                                  .edidState = DisplayIdentity::EdidState::Absent,
                                  .edidIdentifier = {},
                                  .rawEdid = {},
                                  .mstPath = {},
                                  .manufacturer = output.manufacturer,
                                  .model = output.model,
                                  .hasSerial = false,
                                  .internal = output.internal});
    }
    const DisplayIdentity::ResolutionResult identities =
        DisplayIdentity::resolve(identityInputs);
    if (!identities.succeeded()) {
        return failure(InventoryError::IdentityFailure, identities.reasonCode);
    }

    Display::Snapshot snapshot{.protocolVersion = Display::kProtocolVersion,
                               .serviceEpoch = serviceEpoch,
                               .revision = frame.outputGeneration,
                               .liveFingerprint = QByteArray(Display::kFingerprintBytes, '\0'),
                               .outputs = {},
                               .transactions = {},
                               .wireValid = true};
    snapshot.outputs.reserve(frame.outputs.size());
    for (qsizetype index = 0; index < frame.outputs.size(); ++index) {
        const InventoryOutput &input = frame.outputs.at(index);
        const DisplayIdentity::ResolvedOutput &identity = identities.outputs.at(index);
        const QSize pixelSize = currentPixelSize(input);
        if (pixelSize.isEmpty()) {
            return failure(InventoryError::ProjectionFailure,
                           QStringLiteral("current-mode-out-of-range"));
        }
        const Display::Mode mode{.id = currentModeId(pixelSize,
                                                     input.refreshRateMilliHertz),
                                 .pixelSize = pixelSize,
                                 .refreshMilliHertz = input.refreshRateMilliHertz,
                                 .preferred = true};
        if (DisplayTopology::logicalSizeForMode(mode, input.scale, input.transform)
            != input.geometry.size()) {
            return failure(InventoryError::ProjectionFailure,
                           QStringLiteral("current-mode-geometry-mismatch"));
        }
        snapshot.outputs.push_back(
            {.stableId = identity.stableId,
             .connectorName = identity.connectorName,
             .runtimeCompositorUuid = input.runtimeCompositorUuid,
             .label = input.model.isEmpty() ? input.name : input.model,
             .manufacturer = identity.manufacturer,
             .model = identity.model,
             .physicalSizeMillimeters = input.physicalSizeMillimeters,
             .hasSerial = identity.hasSerial,
             .internal = identity.internal,
             .ambiguousIdentity = identity.ambiguous,
             .enabled = true,
             .primary = index == 0,
             .modeId = mode.id,
             .position = input.geometry.topLeft(),
             .logicalSize = input.geometry.size(),
             .scale = input.scale,
             .transform = input.transform,
             .priority = static_cast<quint32>(index + 1),
             .replicationSourceStableId = {},
             .modes = {mode},
             .wireValid = true});
    }

    if (const Display::ValidationResult validation = Display::validateSnapshot(snapshot);
        !validation.accepted) {
        return failure(InventoryError::ProjectionFailure, validation.reasonCode);
    }
    const Display::Candidate projection =
        DisplayTopology::candidateFromSnapshot(snapshot);
    snapshot.liveFingerprint = DisplayTopology::canonicalFingerprint(projection);
    if (const Display::ValidationResult validation = Display::validateSnapshot(snapshot);
        !validation.accepted) {
        return failure(InventoryError::ProjectionFailure, validation.reasonCode);
    }
    return {.snapshot = std::move(snapshot),
            .error = InventoryError::None,
            .reasonCode = {}};
}

} // namespace QindaQt::DisplayService
