// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_identity/identity_registry.h>

namespace QindaQt::DisplayIdentity::Test
{

inline ObservedOutput observed(const QString &connector = QStringLiteral("DP-1"),
                               const QByteArray &identifier = QByteArray("QIN:Panel:SERIAL-SECRET"),
                               const QByteArray &raw = QByteArray::fromHex("00ffffffffffff0011223344"))
{
    return {.connectorName = connector,
            .runtimeCompositorUuid = QStringLiteral("runtime-uuid-a"),
            .edidState = EdidState::Valid,
            .edidIdentifier = identifier,
            .rawEdid = raw,
            .mstPath = {},
            .manufacturer = QStringLiteral("QIN"),
            .model = QStringLiteral("Panel"),
            .hasSerial = true,
            .internal = false};
}

inline Registry registry()
{
    return {.schemaVersion = kRegistrySchemaVersion,
            .entries = {{.stableId = QStringLiteral("edid:00112233445566778899aabbccddeeff"),
                         .alias = QStringLiteral("desk"),
                         .label = QStringLiteral("Desk display"),
                         .lastConnector = QStringLiteral("DP-1"),
                         .manufacturer = QStringLiteral("QIN"),
                         .model = QStringLiteral("Panel"),
                         .internal = false,
                         .ambiguous = false,
                         .seenSequence = 4}}};
}

} // namespace QindaQt::DisplayIdentity::Test
