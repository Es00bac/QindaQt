// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_topology/topology.h>

namespace QindaQt::DisplayTopology::Test
{

inline Display::Output output(const QString &id, const QString &connector,
                              const QPoint &position, const QSize &pixelSize,
                              const double scale, const bool primary,
                              const quint32 priority)
{
    const QString modeId = QStringLiteral("mode-%1x%2").arg(pixelSize.width()).arg(
        pixelSize.height());
    const Display::Mode mode{.id = modeId,
                             .pixelSize = pixelSize,
                             .refreshMilliHertz = 60'000,
                             .preferred = true};
    const QSize logical = logicalSizeForMode(mode, scale, Display::Transform::Normal);
    return {.stableId = id,
            .connectorName = connector,
            .runtimeCompositorUuid = QStringLiteral("runtime-%1").arg(connector),
            .label = connector,
            .manufacturer = QStringLiteral("QIN"),
            .model = QStringLiteral("Panel"),
            .physicalSizeMillimeters = QSize(600, 340),
            .hasSerial = true,
            .internal = false,
            .ambiguousIdentity = false,
            .enabled = true,
            .primary = primary,
            .modeId = modeId,
            .position = position,
            .logicalSize = logical,
            .scale = scale,
            .transform = Display::Transform::Normal,
            .priority = priority,
            .replicationSourceStableId = {},
            .modes = {mode}};
}

inline Display::Snapshot snapshot(QList<Display::Output> outputs, const quint64 revision = 3,
                                  const QString &epoch = QStringLiteral("epoch"))
{
    Display::Snapshot snapshot{.protocolVersion = Display::kProtocolVersion,
                               .serviceEpoch = epoch,
                               .revision = revision,
                               .liveFingerprint = {},
                               .outputs = std::move(outputs),
                               .transactions = {},
                               .wireValid = true};
    snapshot.liveFingerprint = canonicalFingerprint(candidateFromSnapshot(snapshot));
    return snapshot;
}

inline Display::Snapshot singleSnapshot()
{
    return snapshot({output(QStringLiteral("edid:a"), QStringLiteral("DP-1"), {},
                            QSize(2560, 1440), 1.0, true, 1)});
}

inline Display::Candidate candidate(const Display::Snapshot &snapshot)
{
    return candidateFromSnapshot(snapshot);
}

inline Display::Snapshot dualSnapshot()
{
    return snapshot({output(QStringLiteral("edid:a"), QStringLiteral("DP-1"), {},
                            QSize(1920, 1080), 1.0, true, 1),
                     output(QStringLiteral("edid:b"), QStringLiteral("DP-2"),
                            QPoint(1920, 0), QSize(2560, 1440), 1.0, false, 2)});
}

} // namespace QindaQt::DisplayTopology::Test
