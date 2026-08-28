// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility_protocol/wire_limits.h"
#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <optional>

namespace QindaQt::ShellVisibility {

struct CompositorVisibilitySnapshot {
  // The compositor creates a new canonical epoch whenever its publisher
  // lifetime restarts. Revisions are comparable only within this epoch and
  // the D-Bus unique-owner lifetime tracked by the transport-independent
  // acceptance state machine.
  QString epoch;
  quint64 revision = 0;
  quint64 outputGeneration = 0;
  QVector<LogicalOutputSnapshot> outputs;
  QVector<LogicalWindowSnapshot> windows;
  DesktopScopeSnapshot scope;

  friend bool operator==(const CompositorVisibilitySnapshot &,
                         const CompositorVisibilitySnapshot &) = default;
};

enum class CompositorSnapshotErrorCode {
  None,
  PayloadTooLarge,
  InvalidJson,
  InvalidRoot,
  UnsupportedSchema,
  InvalidEpoch,
  InvalidRevision,
  InvalidOutputGeneration,
  CollectionLimitExceeded,
  InvalidField,
  InvalidInventory,
};

struct CompositorSnapshotError {
  CompositorSnapshotErrorCode code = CompositorSnapshotErrorCode::None;
  QString path;
  QString message;

  [[nodiscard]] bool hasError() const noexcept {
    return code != CompositorSnapshotErrorCode::None;
  }
};

struct CompositorSnapshotDecodeResult {
  std::optional<CompositorVisibilitySnapshot> snapshot;
  CompositorSnapshotError error;

  [[nodiscard]] bool ok() const noexcept {
    return snapshot.has_value() && !error.hasError();
  }
};

class CompositorVisibilitySnapshotDecoder final {
public:
  using WireLimits = ShellVisibilityProtocol::WireLimits;
  static constexpr qsizetype MaxPayloadBytes = WireLimits::MaxPayloadBytes;
  static constexpr qsizetype MaxOutputs = WireLimits::MaxOutputs;
  static constexpr qsizetype MaxWindows = WireLimits::MaxWindows;
  static constexpr qsizetype MaxScopeMemberships = WireLimits::MaxScopeMemberships;
  static constexpr qsizetype MaxIdentifierCharacters =
      WireLimits::MaxIdentifierCharacters;
  static constexpr qreal MaxOutputScale = WireLimits::MaxOutputScale;

  [[nodiscard]] static CompositorSnapshotDecodeResult decode(
      const QByteArray &payload);
};

} // namespace QindaQt::ShellVisibility
