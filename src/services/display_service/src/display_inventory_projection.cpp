// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_service/display_inventory.h>

#include <qindaqt/services/display_identity/identity_resolver.h>
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_topology/topology.h>

#include "display_inventory_validation_p.h"

#include <algorithm>
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

// AGENT-CONTRACT: the D4 writer's apply request mapper parses exactly this
// "current:WxH@mHz" form back into a size+refresh mode reference for every
// advertised mode, not only the active one. Changing it breaks mode switches.
// (D2 may not name D4 modules; the boundary test rejects the module token.)
QString modeId(const QSize &pixelSize, quint32 refreshMilliHertz)
{
    return QStringLiteral("current:%1x%2@%3")
        .arg(pixelSize.width())
        .arg(pixelSize.height())
        .arg(refreshMilliHertz);
}

Display::Mode displayMode(const QSize &pixelSize, quint32 refreshMilliHertz,
                          bool preferred)
{
    return {.id = modeId(pixelSize, refreshMilliHertz),
            .pixelSize = pixelSize,
            .refreshMilliHertz = refreshMilliHertz,
            .preferred = preferred};
}

// The advertised list with the current mode guaranteed present. Without a D0
// mode list (older compositor) the current mode is the only entry, as before.
QList<Display::Mode> advertisedModes(const InventoryOutput &input,
                                     const Display::Mode &current)
{
    if (input.modes.isEmpty()) {
        return {current};
    }
    QList<Display::Mode> modes;
    modes.reserve(input.modes.size() + 1);
    bool currentListed = false;
    for (const InventoryMode &mode : input.modes) {
        modes.push_back(displayMode(mode.pixelSize, mode.refreshRateMilliHertz,
                                    mode.preferred));
        currentListed = currentListed || modes.constLast().id == current.id;
    }
    if (!currentListed) {
        if (modes.size() >= Display::kMaxModesPerOutput) {
            modes.removeLast();
        }
        modes.push_back(displayMode(current.pixelSize, current.refreshMilliHertz, false));
    }
    return modes;
}

const InventoryOutput *findOutput(const InventoryFrame &frame, const QString &name)
{
    for (const InventoryOutput &output : frame.outputs) {
        if (output.name == name) {
            return &output;
        }
    }
    return nullptr;
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
    quint32 enabledPriority = 0;
    for (qsizetype index = 0; index < frame.outputs.size(); ++index) {
        const InventoryOutput &input = frame.outputs.at(index);
        const DisplayIdentity::ResolvedOutput &identity = identities.outputs.at(index);
        // A mirror reports its source's geometry with a fitted scale, so only
        // D0's modeSize names its real mode. An extended output keeps the
        // geometry-derived mode unless modeSize reproduces the same geometry,
        // so compositor rounding can never reject an otherwise valid frame.
        const bool mirrored = input.enabled && !input.replicationSource.isEmpty();
        // AGENT-GUARD: only a mirror's KWin-fitted scale is clamped. An
        // extended output's out-of-range scale must still fail validation so
        // a bad frame preserves live truth instead of being silently repaired.
        const double scale = mirrored ? std::clamp(input.scale, Display::kMinimumScale,
                                                   Display::kMaximumScale)
                                      : input.scale;
        QSize pixelSize = currentPixelSize(input);
        if (!input.modePixelSize.isEmpty()
            && (mirrored
                || DisplayTopology::logicalSizeForMode(
                       displayMode(input.modePixelSize, input.refreshRateMilliHertz, true),
                       input.scale, input.transform)
                    == input.geometry.size())) {
            pixelSize = input.modePixelSize;
        }
        if (pixelSize.isEmpty()) {
            return failure(InventoryError::ProjectionFailure,
                           QStringLiteral("current-mode-out-of-range"));
        }
        const Display::Mode mode =
            displayMode(pixelSize, input.refreshRateMilliHertz, input.modes.isEmpty());
        QSize logicalSize = input.geometry.size();
        QPoint position = input.geometry.topLeft();
        QString replicationSourceStableId;
        if (mirrored) {
            const InventoryOutput *source = findOutput(frame, input.replicationSource);
            if (source == nullptr || !source->enabled || !source->replicationSource.isEmpty()) {
                return failure(InventoryError::ProjectionFailure,
                               QStringLiteral("invalid-replication-source"));
            }
            const qsizetype sourceIndex = frame.outputs.indexOf(*source);
            replicationSourceStableId = identities.outputs.at(sourceIndex).stableId;
            position = source->geometry.topLeft();
            // The size it would occupy once extended again at the kept scale.
            logicalSize = DisplayTopology::logicalSizeForMode(mode, scale, input.transform);
        } else if (DisplayTopology::logicalSizeForMode(mode, input.scale, input.transform)
                   != input.geometry.size()) {
            return failure(InventoryError::ProjectionFailure,
                           QStringLiteral("current-mode-geometry-mismatch"));
        }
        if (input.enabled) {
            ++enabledPriority;
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
             .enabled = input.enabled,
             .primary = input.enabled && enabledPriority == 1,
             .modeId = mode.id,
             // Display1 canonicalizes disabled positions. The retained mode,
             // scale, and transform still let Settings build an enable draft.
             .position = input.enabled ? position : QPoint{},
             .logicalSize = logicalSize,
             .scale = scale,
             .transform = input.transform,
             .priority = input.enabled ? enabledPriority : 0,
             .replicationSourceStableId = replicationSourceStableId,
             .modes = advertisedModes(input, mode),
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
