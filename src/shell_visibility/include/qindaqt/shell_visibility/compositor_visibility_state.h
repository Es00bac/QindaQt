// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <optional>

namespace QindaQt::ShellVisibility {

// Tags one asynchronous request with the exact D-Bus service-owner lifetime
// that issued it. The local generation prevents a unique name reused after a
// bus restart from admitting an older reply.
struct CompositorVisibilityRequestTag {
  QString uniqueOwner;
  quint64 ownerGeneration = 0;

  friend bool operator==(const CompositorVisibilityRequestTag &,
                         const CompositorVisibilityRequestTag &) = default;
};

struct CompositorVisibilityLineage {
  CompositorVisibilityRequestTag owner;
  QString epoch;
  quint64 revision = 0;

  friend bool operator==(const CompositorVisibilityLineage &,
                         const CompositorVisibilityLineage &) = default;
};

enum class CompositorVisibilityStateEvent {
  NoChange,
  OwnerChanged,
  SnapshotAccepted,
  SnapshotRecovered,
  SnapshotUnchanged,
  SafeVisibleFallback,
  StaleResponseIgnored,
};

enum class CompositorVisibilityStateErrorCode {
  None,
  InvalidUniqueOwner,
  OwnerGenerationExhausted,
  StaleRequest,
  SnapshotRejected,
  RevisionRegression,
  RevisionCollision,
  TransportFailure,
  ServiceLost,
};

struct CompositorVisibilityStateResult {
  CompositorVisibilityStateEvent event =
      CompositorVisibilityStateEvent::NoChange;
  CompositorVisibilityStateErrorCode code =
      CompositorVisibilityStateErrorCode::None;
  bool stateChanged = false;
  QString message;
  CompositorSnapshotError snapshotError;

  [[nodiscard]] bool ok() const noexcept {
    return code == CompositorVisibilityStateErrorCode::None;
  }

  [[nodiscard]] bool stale() const noexcept {
    return code == CompositorVisibilityStateErrorCode::StaleRequest;
  }
};

// Pure acceptance state for asynchronous compositor snapshot reads. The class
// owns no QObject, timer, or D-Bus handle. A caller observes the current unique
// owner, copies currentRequestTag() onto each request, and returns that exact
// tag with its reply or transport failure.
class CompositorVisibilitySnapshotStateMachine final {
public:
  explicit CompositorVisibilitySnapshotStateMachine(
      quint64 initialOwnerGeneration = 0);

  [[nodiscard]] CompositorVisibilityStateResult
  observeServiceOwner(const QString &uniqueOwner);
  [[nodiscard]] CompositorVisibilityStateResult
  serviceLost(const CompositorVisibilityRequestTag &owner);
  [[nodiscard]] CompositorVisibilityStateResult
  acceptSnapshot(const CompositorVisibilityRequestTag &request,
                 const QByteArray &payload);
  [[nodiscard]] CompositorVisibilityStateResult
  requestFailed(const CompositorVisibilityRequestTag &request,
                QString message);

  [[nodiscard]] std::optional<CompositorVisibilityRequestTag>
  currentRequestTag() const;
  [[nodiscard]] std::optional<CompositorVisibilityLineage>
  lastAcceptedLineage() const;
  [[nodiscard]] const std::optional<CompositorVisibilitySnapshot> &
  snapshot() const noexcept;
  [[nodiscard]] bool safeVisibleRequired() const noexcept;

private:
  [[nodiscard]] bool
  requestIsCurrent(const CompositorVisibilityRequestTag &request) const;
  void clearLineage();
  [[nodiscard]] bool invalidateSnapshot();

  QString m_uniqueOwner;
  quint64 m_ownerGeneration = 0;
  QString m_epoch;
  quint64 m_revision = 0;
  std::optional<CompositorVisibilitySnapshot> m_lastAccepted;
  std::optional<CompositorVisibilitySnapshot> m_snapshot;
  bool m_safeVisibleRequired = true;
};

} // namespace QindaQt::ShellVisibility
