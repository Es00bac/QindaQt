// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_types.h>

namespace QindaQt::Display::Test
{

inline Mode mode(const QString &id = QStringLiteral("2560x1440@60"),
                 const QSize &size = QSize(2560, 1440))
{
    return {.id = id,
            .pixelSize = size,
            .refreshMilliHertz = 60'000,
            .preferred = true};
}

inline Output output(const QString &stableId = QStringLiteral("edid:00112233445566778899aabbccddeeff"),
                     const QString &connector = QStringLiteral("DP-1"))
{
    return {.stableId = stableId,
            .connectorName = connector,
            .runtimeCompositorUuid = QStringLiteral("runtime-only-uuid"),
            .label = QStringLiteral("Example display"),
            .manufacturer = QStringLiteral("QIN"),
            .model = QStringLiteral("Panel"),
            .physicalSizeMillimeters = QSize(600, 340),
            .hasSerial = true,
            .internal = false,
            .ambiguousIdentity = false,
            .enabled = true,
            .primary = true,
            .modeId = QStringLiteral("2560x1440@60"),
            .position = QPoint(0, 0),
            .logicalSize = QSize(2560, 1440),
            .scale = 1.0,
            .transform = Transform::Normal,
            .priority = 1,
            .replicationSourceStableId = {},
            .modes = {mode()}};
}

inline Candidate candidate()
{
    return {.protocolVersion = kProtocolVersion,
            .baseEpoch = QStringLiteral("display-epoch"),
            .baseRevision = 7,
            .outputs = {{.stableId = QStringLiteral("edid:00112233445566778899aabbccddeeff"),
                         .enabled = true,
                         .primary = true,
                         .modeId = QStringLiteral("2560x1440@60"),
                         .position = QPoint(0, 0),
                         .scale = 1.0,
                         .transform = Transform::Normal,
                         .priority = 1,
                         .replicationSourceStableId = {}}}};
}

inline Snapshot snapshot()
{
    return {.protocolVersion = kProtocolVersion,
            .serviceEpoch = QStringLiteral("display-epoch"),
            .revision = 7,
            .liveFingerprint = QByteArray(kFingerprintBytes, '\x2a'),
            .outputs = {output()},
            .transactions = {}};
}

} // namespace QindaQt::Display::Test
