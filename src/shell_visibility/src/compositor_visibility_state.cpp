// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_visibility/compositor_visibility_state.h"

#include <QStringView>

#include <limits>
#include <utility>

namespace QindaQt::ShellVisibility {
namespace {

using ErrorCode = CompositorVisibilityStateErrorCode;
using Event = CompositorVisibilityStateEvent;
using Result = CompositorVisibilityStateResult;

bool validUniqueOwner(QStringView owner) {
  // D-Bus unique names begin with ':' and contain at least two non-empty
  // dot-separated ASCII elements. Unique-name elements may begin with digits.
  if (owner.size() < 4 || owner.size() > 255 || owner.front() != u':') {
    return false;
  }
  bool sawDot = false;
  bool elementHasCharacter = false;
  for (qsizetype index = 1; index < owner.size(); ++index) {
    const ushort character = owner[index].unicode();
    if (character == u'.') {
      if (!elementHasCharacter) {
        return false;
      }
      sawDot = true;
      elementHasCharacter = false;
      continue;
    }
    const bool allowed =
        (character >= u'a' && character <= u'z') ||
        (character >= u'A' && character <= u'Z') ||
        (character >= u'0' && character <= u'9') || character == u'_' ||
        character == u'-';
    if (!allowed) {
      return false;
    }
    elementHasCharacter = true;
  }
  return sawDot && elementHasCharacter;
}

Result staleResult() {
  return {Event::StaleResponseIgnored, ErrorCode::StaleRequest, false,
          QStringLiteral("snapshot result belongs to an older service owner"),
          {}};
}

Result fallbackResult(ErrorCode code, bool changed, QString message,
                      CompositorSnapshotError snapshotError = {}) {
  return {Event::SafeVisibleFallback, code, changed, std::move(message),
          std::move(snapshotError)};
}

} // namespace

CompositorVisibilitySnapshotStateMachine::
    CompositorVisibilitySnapshotStateMachine(quint64 initialOwnerGeneration)
    : m_ownerGeneration(initialOwnerGeneration) {}

CompositorVisibilityStateResult
CompositorVisibilitySnapshotStateMachine::observeServiceOwner(
    const QString &uniqueOwner) {
  if (!validUniqueOwner(uniqueOwner)) {
    const bool hadOwner = !m_uniqueOwner.isEmpty();
    const bool invalidated = invalidateSnapshot();
    const bool changed = hadOwner || invalidated;
    m_uniqueOwner.clear();
    clearLineage();
    return fallbackResult(
        ErrorCode::InvalidUniqueOwner, changed,
        QStringLiteral("compositor service owner is not a canonical D-Bus "
                       "unique name"));
  }
  if (uniqueOwner == m_uniqueOwner) {
    return {};
  }
  if (m_ownerGeneration == std::numeric_limits<quint64>::max()) {
    const bool hadOwner = !m_uniqueOwner.isEmpty();
    const bool invalidated = invalidateSnapshot();
    const bool changed = hadOwner || invalidated;
    m_uniqueOwner.clear();
    clearLineage();
    return fallbackResult(
        ErrorCode::OwnerGenerationExhausted, changed,
        QStringLiteral("compositor service-owner generation is exhausted"));
  }

  ++m_ownerGeneration;
  m_uniqueOwner = uniqueOwner;
  clearLineage();
  const bool changed = invalidateSnapshot();
  return {Event::OwnerChanged, ErrorCode::None, changed, {}, {}};
}

CompositorVisibilityStateResult
CompositorVisibilitySnapshotStateMachine::serviceLost(
    const CompositorVisibilityRequestTag &owner) {
  if (!requestIsCurrent(owner)) {
    return staleResult();
  }
  const bool hadOwner = !m_uniqueOwner.isEmpty();
  const bool invalidated = invalidateSnapshot();
  const bool changed = hadOwner || invalidated;
  m_uniqueOwner.clear();
  clearLineage();
  return fallbackResult(ErrorCode::ServiceLost, changed,
                        QStringLiteral("compositor visibility service was lost"));
}

CompositorVisibilityStateResult
CompositorVisibilitySnapshotStateMachine::acceptSnapshot(
    const CompositorVisibilityRequestTag &request, const QByteArray &payload) {
  if (!requestIsCurrent(request)) {
    return staleResult();
  }

  auto decoded = CompositorVisibilitySnapshotDecoder::decode(payload);
  if (!decoded.ok()) {
    const bool changed = invalidateSnapshot();
    const QString message = decoded.error.message.isEmpty()
                                ? QStringLiteral("compositor snapshot was rejected")
                                : decoded.error.message;
    return fallbackResult(ErrorCode::SnapshotRejected, changed, message,
                          std::move(decoded.error));
  }

  CompositorVisibilitySnapshot candidate = std::move(*decoded.snapshot);
  if (!m_lastAccepted || candidate.epoch != m_epoch) {
    const bool changed = m_safeVisibleRequired || !m_snapshot.has_value() ||
        candidate != m_snapshot.value_or(CompositorVisibilitySnapshot{});
    m_epoch = candidate.epoch;
    m_revision = candidate.revision;
    m_lastAccepted = candidate;
    m_snapshot = std::move(candidate);
    m_safeVisibleRequired = false;
    return {Event::SnapshotAccepted, ErrorCode::None, changed, {}, {}};
  }

  if (candidate.revision < m_revision) {
    const bool changed = invalidateSnapshot();
    return fallbackResult(
        ErrorCode::RevisionRegression, changed,
        QStringLiteral("compositor snapshot revision regressed within one "
                       "owner and epoch"));
  }
  if (candidate.revision == m_revision) {
    if (candidate != *m_lastAccepted) {
      const bool changed = invalidateSnapshot();
      return fallbackResult(
          ErrorCode::RevisionCollision, changed,
          QStringLiteral("one compositor revision encoded different complete "
                         "snapshot values"));
    }
    if (m_snapshot.has_value() && !m_safeVisibleRequired) {
      return {Event::SnapshotUnchanged, ErrorCode::None, false, {}, {}};
    }
    m_snapshot = candidate;
    m_safeVisibleRequired = false;
    return {Event::SnapshotRecovered, ErrorCode::None, true, {}, {}};
  }

  const bool changed = m_safeVisibleRequired || !m_snapshot.has_value() ||
      candidate != m_snapshot.value_or(CompositorVisibilitySnapshot{});
  m_revision = candidate.revision;
  m_lastAccepted = candidate;
  m_snapshot = std::move(candidate);
  m_safeVisibleRequired = false;
  return {Event::SnapshotAccepted, ErrorCode::None, changed, {}, {}};
}

CompositorVisibilityStateResult
CompositorVisibilitySnapshotStateMachine::requestFailed(
    const CompositorVisibilityRequestTag &request, QString message) {
  if (!requestIsCurrent(request)) {
    return staleResult();
  }
  const bool changed = invalidateSnapshot();
  if (message.trimmed().isEmpty()) {
    message = QStringLiteral("compositor snapshot request failed");
  }
  return fallbackResult(ErrorCode::TransportFailure, changed,
                        std::move(message));
}

std::optional<CompositorVisibilityRequestTag>
CompositorVisibilitySnapshotStateMachine::currentRequestTag() const {
  if (m_uniqueOwner.isEmpty() || m_ownerGeneration == 0) {
    return std::nullopt;
  }
  return CompositorVisibilityRequestTag{m_uniqueOwner, m_ownerGeneration};
}

std::optional<CompositorVisibilityLineage>
CompositorVisibilitySnapshotStateMachine::lastAcceptedLineage() const {
  const auto owner = currentRequestTag();
  if (!owner || !m_lastAccepted) {
    return std::nullopt;
  }
  return CompositorVisibilityLineage{*owner, m_epoch, m_revision};
}

const std::optional<CompositorVisibilitySnapshot> &
CompositorVisibilitySnapshotStateMachine::snapshot() const noexcept {
  return m_snapshot;
}

bool CompositorVisibilitySnapshotStateMachine::safeVisibleRequired() const
    noexcept {
  return m_safeVisibleRequired;
}

bool CompositorVisibilitySnapshotStateMachine::requestIsCurrent(
    const CompositorVisibilityRequestTag &request) const {
  return !m_uniqueOwner.isEmpty() && request.ownerGeneration != 0 &&
         request.ownerGeneration == m_ownerGeneration &&
         request.uniqueOwner == m_uniqueOwner;
}

void CompositorVisibilitySnapshotStateMachine::clearLineage() {
  m_epoch.clear();
  m_revision = 0;
  m_lastAccepted.reset();
}

bool CompositorVisibilitySnapshotStateMachine::invalidateSnapshot() {
  const bool changed = m_snapshot.has_value() || !m_safeVisibleRequired;
  m_snapshot.reset();
  m_safeVisibleRequired = true;
  return changed;
}

} // namespace QindaQt::ShellVisibility
