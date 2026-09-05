// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launcher_persistence.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <qindaqt/services/settings_client/settings_client.h>

#include <QVariant>
#include <QVariantList>

namespace QindaQt::Shell::Launcher {

using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::CommitOutcome;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::ShellLauncher::Bounds::maxPinnedEntries;
using QindaQt::ShellLauncher::Bounds::maxRecentEntries;
using QindaQt::ShellLauncher::Bounds::isValidEntryId;
using QindaQt::ShellLauncher::PinError;

bool normalizeStoredIdList(const QVariant &stored, int ceiling, QStringList *ids)
{
  if (!stored.isValid() || stored.isNull()
      || stored.metaType().id() == QMetaType::Nullptr) {
    ids->clear();
    return true; // absent key: the empty list
  }
  if (stored.metaType().id() != QMetaType::QVariantList
      && stored.metaType().id() != QMetaType::QStringList) {
    return false;
  }
  QStringList result;
  for (const QVariant &item : stored.toList()) {
    if (item.metaType().id() != QMetaType::QString)
      return false;
    const QString id = item.toString();
    // AGENT-GUARD: Only desktop-entry ids are ever stored. A malformed
    // element poisons the whole key so a hostile or corrupted value can
    // never smuggle non-identity data into the models.
    if (!isValidEntryId(id) || result.contains(id))
      return false;
    result.append(id);
    if (result.size() > ceiling)
      return false;
  }
  *ids = result;
  return true;
}

LauncherPersistenceController::LauncherPersistenceController(SettingsClient &client,
                                                             QObject *parent)
    : QObject(parent), m_client(client)
{
  connect(&m_client, &SettingsClient::snapshotChanged,
          this, &LauncherPersistenceController::handleSnapshot);
  connect(&m_client, &SettingsClient::commitFinished,
          this, &LauncherPersistenceController::handleCommit);
  connect(&m_client, &SettingsClient::commitUncertain,
          this, &LauncherPersistenceController::handleUncertain);
  connect(&m_client, &SettingsClient::stateChanged,
          this, &LauncherPersistenceController::handleClientState);
  handleSnapshot();
  handleClientState();
}

bool LauncherPersistenceController::persistenceReady() const noexcept
{
  return m_confirmedBaseline && m_client.state() == ClientState::Ready;
}

void LauncherPersistenceController::handleSnapshot()
{
  const auto &snapshot = m_client.snapshot();
  if (!snapshot) {
    clearAuthoritativeTruth();
    return;
  }
  // A snapshot that arrives while a commit is in flight is the optimistic
  // base, not new authority; the commit result settles the truth.
  if (writeInFlight())
    return;

  // A startup/owner-loss notice describes temporary connectivity, not a
  // failed save. Retire it on confirmed recovery before validating the lists;
  // malformed-list and operation errors retain their own useful explanation.
  if (m_availabilityNotice && m_client.state() == ClientState::Ready)
    setStatusText({});

  QStringList pinned;
  if (!normalizeStoredIdList(snapshot->values.value(pinnedKey()),
                             maxPinnedEntries, &pinned)) {
    pinned.clear();
    setStatusText(QStringLiteral("Stored pinned list is malformed and was ignored"));
  }
  QStringList recent;
  if (!normalizeStoredIdList(snapshot->values.value(recentKey()),
                             maxRecentEntries, &recent)) {
    recent.clear();
    setStatusText(QStringLiteral("Stored recent list is malformed and was ignored"));
  }

  // AGENT-GUARD: Compare authority with live optimistic state, not only the
  // previous confirmed snapshot. After an uncertain commit the authoritative
  // resync may be byte-for-byte unchanged while the live model still contains
  // the unconfirmed draft; it must converge without replay (ADR-0012).
  const bool pinnedDidChange = pinned != m_pinned.ids();
  const bool recentDidChange = recent != m_recent.ids();
  m_confirmedPinned = pinned;
  m_confirmedRecent = recent;
  m_confirmedBaseline = true;
  if (pinnedDidChange) {
    m_pinned = PinnedApplications();
    for (const QString &id : pinned)
      m_pinned.pin(id); // validated above; cannot fail
    Q_EMIT pinnedChanged();
  }
  if (recentDidChange) {
    m_recent = RecentApplications();
    for (auto it = recent.crbegin(); it != recent.crend(); ++it)
      m_recent.record(*it); // record() fronts; reverse replays original order
    Q_EMIT recentChanged();
  }
  Q_EMIT stateChanged();
}

void LauncherPersistenceController::handleCommit(const CommitOutcome &outcome)
{
  if (m_pendingKey.isEmpty())
    return;
  const QString key = m_pendingKey;
  m_pendingKey.clear();
  if (outcome.status == SettingsWireStatus::Applied) {
    if (key == pinnedKey())
      m_confirmedPinned = m_pinned.ids();
    else
      m_confirmedRecent = m_recent.ids();
    setStatusText({});
  } else {
    // AGENT-GUARD: A confirmed rejection never leaves the local model
    // claiming a save that did not happen (ADR-0012); the last confirmed
    // value comes back and the reason stays visible until the next write.
    revertToConfirmed(key);
    setStatusText(outcome.message.isEmpty()
                      ? QStringLiteral("The launcher setting could not be saved")
                      : outcome.message);
  }
  Q_EMIT stateChanged();
}

void LauncherPersistenceController::handleUncertain(const QString &message)
{
  if (m_pendingKey.isEmpty())
    return;
  m_pendingKey.clear();
  // Never replay: the client's resync snapshot will converge the models.
  setStatusText(message.isEmpty()
                    ? QStringLiteral("The save outcome is uncertain; resynchronizing")
                    : message);
  Q_EMIT stateChanged();
}

void LauncherPersistenceController::handleClientState()
{
  if (m_client.state() != ClientState::Ready) {
    // AGENT-GUARD: Pinned/recent identities are Settings1-owner truth. Keeping
    // them whenever the client lacks Ready authority would let an owner loss,
    // replacement, or malformed resync preserve stale presentation state.
    clearAuthoritativeTruth();
  }
  if (m_client.state() == ClientState::Unavailable && !writeInFlight()
      && (m_statusText.isEmpty() || m_availabilityNotice)) {
    setStatusText(QStringLiteral("Settings persistence is unavailable"), true);
  }
  Q_EMIT stateChanged();
}

void LauncherPersistenceController::clearAuthoritativeTruth()
{
  m_confirmedBaseline = false;
  m_confirmedPinned.clear();
  m_confirmedRecent.clear();
  if (!m_pinned.ids().isEmpty()) {
    m_pinned = PinnedApplications();
    Q_EMIT pinnedChanged();
  }
  if (!m_recent.ids().isEmpty()) {
    m_recent = RecentApplications();
    Q_EMIT recentChanged();
  }
}

void LauncherPersistenceController::revertToConfirmed(const QString &key)
{
  if (key == pinnedKey()) {
    PinnedApplications restored;
    for (const QString &id : m_confirmedPinned)
      restored.pin(id);
    m_pinned = restored;
    Q_EMIT pinnedChanged();
  } else {
    RecentApplications restored;
    for (auto it = m_confirmedRecent.crbegin(); it != m_confirmedRecent.crend(); ++it)
      restored.record(*it);
    m_recent = restored;
    Q_EMIT recentChanged();
  }
}

void LauncherPersistenceController::setStatusText(const QString &text, bool availabilityNotice)
{
  if (m_statusText == text && m_availabilityNotice == availabilityNotice)
    return;
  m_statusText = text;
  m_availabilityNotice = availabilityNotice;
  Q_EMIT stateChanged();
}

PersistenceMutation LauncherPersistenceController::commitList(const QString &key,
                                                              const QStringList &ids)
{
  QVariantList values;
  values.reserve(ids.size());
  for (const QString &id : ids)
    values.append(id);
  QString error;
  if (!m_client.setUserValue(key, QVariant::fromValue(values), &error)) {
    revertToConfirmed(key);
    setStatusText(error.isEmpty()
                      ? QStringLiteral("Settings persistence is unavailable")
                      : error, error.isEmpty());
    Q_EMIT stateChanged();
    return PersistenceMutation::Unavailable;
  }
  m_pendingKey = key;
  setStatusText({});
  Q_EMIT stateChanged();
  return PersistenceMutation::Applied;
}

PersistenceMutation LauncherPersistenceController::mutatePinned(
    const QString &key, PinError (PinnedApplications::*op)(const QString &),
    const QString &entryId)
{
  if (!persistenceReady())
    return PersistenceMutation::Unavailable;
  if (writeInFlight())
    return PersistenceMutation::Busy;
  if ((m_pinned.*op)(entryId) != PinError::None)
    return PersistenceMutation::RejectedByModel;
  const PersistenceMutation result = commitList(key, m_pinned.ids());
  if (result == PersistenceMutation::Applied)
    Q_EMIT pinnedChanged();
  return result;
}

PersistenceMutation LauncherPersistenceController::pin(const QString &entryId)
{
  return mutatePinned(pinnedKey(), &PinnedApplications::pin, entryId);
}

PersistenceMutation LauncherPersistenceController::unpin(const QString &entryId)
{
  return mutatePinned(pinnedKey(), &PinnedApplications::unpin, entryId);
}

PersistenceMutation LauncherPersistenceController::movePinnedUp(const QString &entryId)
{
  return mutatePinned(pinnedKey(), &PinnedApplications::moveUp, entryId);
}

PersistenceMutation LauncherPersistenceController::movePinnedDown(const QString &entryId)
{
  return mutatePinned(pinnedKey(), &PinnedApplications::moveDown, entryId);
}

PersistenceMutation LauncherPersistenceController::clearRecent()
{
  if (!persistenceReady())
    return PersistenceMutation::Unavailable;
  if (writeInFlight())
    return PersistenceMutation::Busy;
  m_recent.clear();
  const PersistenceMutation result = commitList(recentKey(), {});
  if (result == PersistenceMutation::Applied)
    Q_EMIT recentChanged();
  return result;
}

PersistenceMutation LauncherPersistenceController::recordLaunch(const QString &entryId)
{
  if (!persistenceReady() || writeInFlight())
    return writeInFlight() ? PersistenceMutation::Busy
                           : PersistenceMutation::Unavailable;
  if (m_recent.record(entryId) != QindaQt::ShellLauncher::RecentError::None)
    return PersistenceMutation::RejectedByModel;
  const PersistenceMutation result = commitList(recentKey(), m_recent.ids());
  if (result == PersistenceMutation::Applied)
    Q_EMIT recentChanged();
  return result;
}

} // namespace QindaQt::Shell::Launcher
