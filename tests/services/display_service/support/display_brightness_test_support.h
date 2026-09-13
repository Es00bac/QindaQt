// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_service/display_service_ports.h>

#include <QtTest/QTest>

#include <algorithm>

// Compares an immediate brightness result and proves it is a valid protocol
// value. QTest macros return from the calling test function on failure.
#define QINDAQT_VERIFY_IMMEDIATE(result, s, e, d)                                   \
    do {                                                                            \
        QCOMPARE((result).status, s);                                               \
        QCOMPARE((result).error, e);                                                \
        QCOMPARE((result).diagnostic, QString::fromLatin1(d));                      \
        QCOMPARE((result).kind, QindaQt::Display::OperationKind::ImmediatePolicy);  \
        QVERIFY(QindaQt::Display::validateOperationResult(result).accepted);        \
    } while (false)

namespace QindaQt::DisplayService::TestSupport::Brightness
{

inline const QString kExternal = QStringLiteral("conn:DP-1");
inline const QString kInternal = QStringLiteral("conn:eDP-1");

inline Display::Output topologyOutput(const QString &connector, const QString &uuid,
                                      const quint32 priority)
{
    const Display::Mode mode{.id = QStringLiteral("current:1920x1080@60000"),
                             .pixelSize = QSize(1920, 1080),
                             .refreshMilliHertz = 60'000,
                             .preferred = true};
    return {.stableId = QStringLiteral("conn:%1").arg(connector),
            .connectorName = connector,
            .runtimeCompositorUuid = uuid,
            .label = connector,
            .manufacturer = QStringLiteral("QIN"),
            .model = QStringLiteral("Panel"),
            .physicalSizeMillimeters = QSize(600, 340),
            .hasSerial = false,
            .internal = false,
            .ambiguousIdentity = false,
            .enabled = true,
            .primary = priority == 1,
            .modeId = mode.id,
            .position = QPoint(static_cast<int>(priority - 1) * 1920, 0),
            .logicalSize = QSize(1920, 1080),
            .scale = 1.0,
            .transform = Display::Transform::Normal,
            .priority = priority,
            .replicationSourceStableId = {},
            .modes = {mode},
            .wireValid = true};
}

// An external DP-1 and an internal eDP-1 in one accepted epoch.
inline Display::Snapshot topology(const quint64 revision = 4)
{
    Display::Output internal =
        topologyOutput(QStringLiteral("eDP-1"), QStringLiteral("uuid-edp"), 2);
    internal.internal = true;
    return {.protocolVersion = Display::kProtocolVersion,
            .serviceEpoch = QStringLiteral("epoch-a"),
            .revision = revision,
            .liveFingerprint = {},
            .outputs = {topologyOutput(QStringLiteral("DP-1"), QStringLiteral("uuid-dp"), 1),
                        internal},
            .transactions = {},
            .wireValid = true};
}

inline DeviceBrightness device(const QString &connector, const QString &uuid,
                               const quint32 value)
{
    return {.connectorName = connector,
            .runtimeUuid = uuid,
            .enabled = true,
            .capable = true,
            .observed = true,
            .value = value};
}

inline DeviceBrightnessFrame devices(const quint64 generation = 5)
{
    return {.ownerGeneration = generation,
            .devices = {device(QStringLiteral("DP-1"), QStringLiteral("uuid-dp"), 6'000),
                        device(QStringLiteral("eDP-1"), QStringLiteral("uuid-edp"), 3'000)}};
}

inline DeviceBrightnessFrame withExternalValue(const quint32 value, const quint64 generation = 5)
{
    DeviceBrightnessFrame frame = devices(generation);
    frame.devices[0].value = value;
    return frame;
}

inline Display::BrightnessRequest requestFor(const Display::BrightnessSnapshot &published,
                                             const QString &stableId, const quint32 value)
{
    return {.baseEpoch = published.serviceEpoch,
            .baseRevision = published.revision,
            .stableId = stableId,
            .value = value};
}

inline QString stableIdFor(const Display::Snapshot &snapshot, const QString &connector)
{
    const auto found =
        std::ranges::find(snapshot.outputs, connector, &Display::Output::connectorName);
    return found == snapshot.outputs.cend() ? QString{} : found->stableId;
}

} // namespace QindaQt::DisplayService::TestSupport::Brightness
