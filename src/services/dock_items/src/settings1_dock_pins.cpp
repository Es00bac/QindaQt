// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/dock_items/settings1_dock_pins.h>

#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QVariant>

namespace QindaQt::Services::DockItems {
namespace {

using SettingsClient::ClientState;
using SettingsProtocol::SettingsWireStatus;

constexpr int kReadbackPollMilliseconds = 100;
constexpr int kReadbackDeadlineMilliseconds = 3'000;

// The largest admissible dock (every item a maximal path, three UTF-8 bytes
// per UTF-16 unit, plus field overhead) must fit one Settings1 value, or the
// codec would admit docks the service refuses to store.
static_assert(Bounds::maxItems * (Bounds::maxPathLength * 3 + 64)
                  < SettingsProtocol::WireContract::MaximumAggregateValueBytes,
              "dock bounds exceed the Settings1 value bound");

QString refusalText(DockEditError error, bool pin)
{
  switch (error) {
  case DockEditError::AlreadyInDock:
    return QStringLiteral("The application is already in the Dock.");
  case DockEditError::NotInDock:
    return QStringLiteral("The application is not in the Dock.");
  case DockEditError::DockFull:
    return QStringLiteral("The Dock is full.");
  case DockEditError::InvalidItem:
    return QStringLiteral("That application cannot be kept in the Dock.");
  case DockEditError::GroupFull:
  case DockEditError::OutOfRange:
  case DockEditError::WrongKind:
  case DockEditError::None:
    break;
  }
  return pin ? QStringLiteral("The application could not be added to the Dock.")
             : QStringLiteral("The application could not be removed from the Dock.");
}

} // namespace

const QStringList &Settings1DockPins::scopedKeys()
{
  static const QStringList keys{QLatin1StringView(DockItemsSettingsKey),
                                QLatin1StringView(LegacyPinnedSettingsKey)};
  return keys;
}

Settings1DockPins::Settings1DockPins(SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client)
{
  connect(&m_client, &SettingsClient::SettingsClient::snapshotChanged, this,
          &Settings1DockPins::onSnapshotChanged);
  connect(&m_client, &SettingsClient::SettingsClient::stateChanged, this,
          &Settings1DockPins::onStateChanged);
  // An owner may be replaced Authenticating-to-Authenticating without a
  // stateChanged signal; the pending write must still notice.
  connect(&m_client, &SettingsClient::SettingsClient::ownerChanged, this,
          &Settings1DockPins::onStateChanged);
  connect(&m_client, &SettingsClient::SettingsClient::commitFinished, this,
          &Settings1DockPins::onCommitFinished);
  connect(&m_client, &SettingsClient::SettingsClient::commitUncertain, this,
          &Settings1DockPins::onCommitUncertain);
  m_readbackTimer.setInterval(kReadbackPollMilliseconds);
  connect(&m_readbackTimer, &QTimer::timeout, this, &Settings1DockPins::onReadbackTick);
  onSnapshotChanged();
}

bool Settings1DockPins::isLoaded() const
{
  const auto &snapshot = m_client.snapshot();
  // A commit's readback briefly returns the client to Authenticating for the
  // same owner; the last confirmed dock stays usable there. Owner loss,
  // replacement, and degradation invalidate it.
  return m_loaded && m_client.state() != ClientState::Unavailable
      && m_client.state() != ClientState::Degraded && snapshot
      && !m_client.currentOwner().isEmpty() && snapshot->owner == m_client.currentOwner();
}

bool Settings1DockPins::isPinned(const QString &applicationId) const
{
  return isLoaded() && m_dock && m_dock->indexOfApplication(applicationId) >= 0;
}

bool Settings1DockPins::pinApplication(const QString &applicationId)
{
  return request(applicationId, true);
}

bool Settings1DockPins::unpinApplication(const QString &applicationId)
{
  return request(applicationId, false);
}

bool Settings1DockPins::request(const QString &applicationId, bool pin)
{
  if (!isLoaded() || m_pending) {
    setStatus(QStringLiteral("The Dock is unavailable or another change is pending."));
    return false;
  }
  if (!m_dock) {
    setStatus(QStringLiteral("The stored Dock could not be read, so it was left unchanged."));
    return false;
  }
  DockItems next = *m_dock;
  const DockEditError error = pin
      ? next.insert(next.size(), DockItem::application(applicationId))
      : next.removeApplication(applicationId);
  if (error != DockEditError::None) {
    setStatus(refusalText(error, pin));
    return false;
  }
  QString clientError;
  if (!m_client.setUserValue(QLatin1StringView(DockItemsSettingsKey),
                             DockItems::encodeSettingsValue(next), &clientError)) {
    setStatus(clientError.isEmpty() ? QStringLiteral("Settings could not accept the change.")
                                    : clientError);
    return false;
  }
  Pending pending;
  pending.applicationId = applicationId;
  pending.pin = pin;
  pending.requested = next;
  pending.owner = m_client.currentOwner();
  pending.epoch = m_client.snapshot() ? m_client.snapshot()->epoch : QString{};
  m_pending = pending;
  setStatus(pin ? QStringLiteral("Adding to the Dock…")
                : QStringLiteral("Removing from the Dock…"));
  return true;
}

void Settings1DockPins::onSnapshotChanged()
{
  const auto &snapshot = m_client.snapshot();
  if (!snapshot || snapshot->owner != m_client.currentOwner()
      || m_client.state() != ClientState::Ready) {
    return;
  }
  const std::optional<DockItems> dock = DockItems::fromSettingsValues(snapshot->values);
  const bool changed = !m_loaded || dock != m_dock;
  m_loaded = true;
  m_dock = dock;
  if (changed)
    Q_EMIT pinsChanged();
  if (!m_pending || !m_pending->awaitingReadback)
    return;
  // AGENT-GUARD: the client accepts an unchanged older revision while the
  // service is still exposing the Applied commit. Only the original owner
  // and epoch at or beyond revisionAfter can settle the request.
  if (snapshot->owner != m_pending->owner || snapshot->epoch != m_pending->epoch) {
    finish(Outcome::Uncertain,
           QStringLiteral("The Dock change was not confirmed after the settings service changed."));
    return;
  }
  if (snapshot->revision < m_pending->revisionFloor)
    return;
  if (dock && *dock == m_pending->requested)
    finish(Outcome::Saved, {});
  else
    finish(Outcome::Conflict,
           QStringLiteral("The Dock changed elsewhere before this change was confirmed."));
}

void Settings1DockPins::onStateChanged()
{
  if (isLoaded())
    return;
  if (m_pending) {
    finish(Outcome::Uncertain,
           QStringLiteral("The Dock change was not confirmed. Check the Dock before retrying."));
  }
  if (m_loaded) {
    m_loaded = false;
    Q_EMIT pinsChanged();
  }
}

void Settings1DockPins::onCommitFinished(const SettingsClient::CommitOutcome &outcome)
{
  if (!m_pending || m_pending->awaitingReadback)
    return;
  if (outcome.status == SettingsWireStatus::Applied) {
    const auto &baseline = m_client.snapshot();
    if (!baseline || baseline->owner != m_pending->owner
        || baseline->epoch != m_pending->epoch) {
      finish(Outcome::Uncertain,
             QStringLiteral("The Dock change was accepted by a settings service that has since changed."));
      return;
    }
    m_pending->revisionFloor = outcome.revisionAfter;
    m_pending->awaitingReadback = true;
    m_readbackAge.start();
    m_readbackTimer.start();
    setStatus(QStringLiteral("Checking the saved Dock…"));
    return;
  }
  const bool conflict = outcome.status == SettingsWireStatus::Conflict
      || outcome.status == SettingsWireStatus::EpochMismatch;
  finish(conflict ? Outcome::Conflict : Outcome::Refused,
         outcome.message.isEmpty() ? QStringLiteral("Settings refused the Dock change.")
                                   : outcome.message.left(512));
}

void Settings1DockPins::onCommitUncertain(const QString &message)
{
  if (!m_pending)
    return;
  finish(Outcome::Uncertain,
         message.isEmpty()
             ? QStringLiteral("The Dock change was not confirmed. Check the Dock before retrying.")
             : QStringLiteral("The Dock change was not confirmed: %1").arg(message.left(512)));
}

void Settings1DockPins::onReadbackTick()
{
  if (!m_pending || !m_pending->awaitingReadback) {
    m_readbackTimer.stop();
    return;
  }
  if (m_client.currentOwner() != m_pending->owner) {
    finish(Outcome::Uncertain,
           QStringLiteral("The Dock change was not confirmed after the settings service changed."));
    return;
  }
  if (m_readbackAge.elapsed() >= kReadbackDeadlineMilliseconds) {
    finish(Outcome::Uncertain,
           QStringLiteral("The Dock change was accepted but not confirmed. Check the Dock before retrying."));
    return;
  }
  // A legal same-revision snapshot may predate the Applied revision.
  // Refetch only; never replay the write.
  if (m_client.state() == ClientState::Ready)
    m_client.refresh();
}

void Settings1DockPins::finish(Outcome outcome, const QString &status)
{
  if (!m_pending)
    return;
  const Pending pending = *m_pending;
  m_pending.reset();
  m_readbackTimer.stop();
  setStatus(status);
  Q_EMIT requestFinished(pending.applicationId, pending.pin, outcome);
}

void Settings1DockPins::setStatus(const QString &status)
{
  if (m_status == status)
    return;
  m_status = status;
  Q_EMIT statusChanged();
}

} // namespace QindaQt::Services::DockItems
