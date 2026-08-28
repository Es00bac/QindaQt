// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QByteArrayView>
#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QString>

namespace QindaQt::DisplayService
{

inline constexpr qsizetype kMaximumCompositorInventoryBytes = 4'194'304;

enum class InventoryError {
    None,
    PayloadTooLarge,
    MalformedPayload,
    UnsupportedSchema,
    Unavailable,
    InvalidOwner,
    InvalidGeneration,
    InvalidOutput,
    IdentityFailure,
    ProjectionFailure,
};

struct InventoryOutput {
    QString name;
    QRect geometry;
    double scale = 1.0;
    quint32 refreshRateMilliHertz = 0;
    Display::Transform transform = Display::Transform::Normal;
    bool internal = false;
    QString runtimeCompositorUuid;
    quint32 compositorPriority = 0;
    QSize physicalSizeMillimeters;
    QString manufacturer;
    QString model;

    friend bool operator==(const InventoryOutput &, const InventoryOutput &) = default;
};

struct InventoryFrame {
    QString uniqueOwner;
    quint64 outputGeneration = 0;
    QList<InventoryOutput> outputs;

    friend bool operator==(const InventoryFrame &, const InventoryFrame &) = default;
};

struct InventoryDecodeResult {
    InventoryFrame frame;
    InventoryError error = InventoryError::None;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept { return error == InventoryError::None; }
};

struct InventoryProjectionResult {
    Display::Snapshot snapshot;
    InventoryError error = InventoryError::None;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept { return error == InventoryError::None; }
};

// The payload and owner are borrowed for one call; the result owns bounded
// values. Decoding is total, reentrant, and never mutates an earlier frame.
// The owner must be the exact D-Bus unique owner from which the call replied.
[[nodiscard]] InventoryDecodeResult decodeCompositorInventory(
    QByteArrayView payload, const QString &uniqueOwner);

// AGENT-CONTRACT: projection never treats runtimeCompositorUuid as persistent
// identity. D0 has no EDID or MST material, so D1 connector fallback is the
// only stable-ID authority in this slice. The returned snapshot is complete,
// validation-accepted, and fingerprinted by display_topology or is empty.
[[nodiscard]] InventoryProjectionResult projectInventory(
    const InventoryFrame &frame, const QString &serviceEpoch);

} // namespace QindaQt::DisplayService
