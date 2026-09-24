// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launcher_persistence.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <qindaqt/services/settings_client/settings_client.h>

#include <QVariant>
#include <QVariantList>

namespace QindaQt::Shell::Launcher {

using QindaQt::Services::DockItems::DockEditError;
using QindaQt::Services::DockItems::DockItem;
using QindaQt::Services::DockItems::DockItems;
using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::CommitOutcome;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::ShellLauncher::Bounds::maxRecentEntries;
using QindaQt::ShellLauncher::Bounds::isValidEntryId;

namespace {

QVariantList idValues(const QStringList &ids)
{
  QVariantList values;
  values.reserve(ids.size());
  for (const QString &id : ids)
    values.append(id);
  return values;
}

} // namespace

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

  // ADR-0265: a migrated dock value is the pin authority; until the first
  // dock write the dock is derived from the legacy id list, which keeps its
  // whole-list rejection. Either malformed value reads as an empty dock with
  // visible truth, and the next explicit edit replaces it.
  DockItems dock;
  const auto decodedDock =
      DockItems::decodeSettingsValue(snapshot->values.value(dockItemsKey()));
  if (!decodedDock.ok()) {
    setStatusText(QStringLiteral("Stored dock items are malformed and were ignored"));
  } else if (!decodedDock.unmigrated) {
    dock = *decodedDock.items;
  } else if (const auto legacy =
                 DockItems::fromLegacyValue(snapshot->values.value(pinnedKey()))) {
    dock = *legacy;
  } else {
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
  const bool dockDidChange = dock != m_dock;
  const bool recentDidChange = recent != m_recent.ids();
  m_confirmedDock = dock;
  m_confirmedRecent = recent;
  m_confirmedBaseline = true;
  if (dockDidChange)
    adoptDock(dock);
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
    if (key == dockItemsKey())
      m_confirmedDock = m_dock;
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
  m_confirmedDock = DockItems();
  m_confirmedRecent.clear();
  if (!m_dock.isEmpty())
    adoptDock(DockItems());
  if (!m_recent.ids().isEmpty()) {
    m_recent = RecentApplications();
    Q_EMIT recentChanged();
  }
}

void LauncherPersistenceController::adoptDock(const DockItems &dock)
{
  m_dock = dock;
  PinnedApplications pinned;
  for (const QString &id : m_dock.applicationIds())
    pinned.pin(id); // the dock shares the pinned ceiling; cannot fail
  const bool pinnedDidChange = pinned.ids() != m_pinned.ids();
  m_pinned = pinned;
  Q_EMIT dockChanged();
  if (pinnedDidChange)
    Q_EMIT pinnedChanged();
}

void LauncherPersistenceController::revertToConfirmed(const QString &key)
{
  if (key == dockItemsKey()) {
    adoptDock(m_confirmedDock);
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

PersistenceMutation LauncherPersistenceController::commitValue(const QString &key,
                                                               const QVariant &value)
{
  QString error;
  if (!m_client.setUserValue(key, value, &error)) {
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

PersistenceMutation LauncherPersistenceController::editDock(const DockEdit &edit)
{
  if (!persistenceReady())
    return PersistenceMutation::Unavailable;
  if (writeInFlight())
    return PersistenceMutation::Busy;
  DockItems next = m_dock;
  m_lastDockEditError = edit(next);
  if (m_lastDockEditError != DockEditError::None)
    return PersistenceMutation::RejectedByModel;
  // AGENT-GUARD: the live dock changes before the commit resolves (ADR-0012);
  // a refusal or an unavailable client reverts to m_confirmedDock.
  adoptDock(next);
  return commitValue(dockItemsKey(), DockItems::encodeSettingsValue(m_dock));
}

PersistenceMutation LauncherPersistenceController::pin(const QString &entryId)
{
  return editDock([&entryId](DockItems &dock) {
    return dock.insert(dock.size(), DockItem::application(entryId));
  });
}

PersistenceMutation LauncherPersistenceController::unpin(const QString &entryId)
{
  return editDock([&entryId](DockItems &dock) { return dock.removeApplication(entryId); });
}

PersistenceMutation LauncherPersistenceController::movePinnedUp(const QString &entryId)
{
  return editDock([&entryId](DockItems &dock) {
    const qsizetype index = dock.indexOfApplication(entryId);
    if (index < 0)
      return DockEditError::NotInDock;
    return index == 0 ? DockEditError::None : dock.moveToGap(index, index - 1);
  });
}

PersistenceMutation LauncherPersistenceController::movePinnedDown(const QString &entryId)
{
  return editDock([&entryId](DockItems &dock) {
    const qsizetype index = dock.indexOfApplication(entryId);
    if (index < 0)
      return DockEditError::NotInDock;
    return index + 1 >= dock.size() ? DockEditError::None
                                    : dock.moveToGap(index, index + 2);
  });
}

PersistenceMutation LauncherPersistenceController::clearRecent()
{
  if (!persistenceReady())
    return PersistenceMutation::Unavailable;
  if (writeInFlight())
    return PersistenceMutation::Busy;
  m_recent.clear();
  const PersistenceMutation result = commitValue(recentKey(), QVariantList{});
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
  const PersistenceMutation result = commitValue(recentKey(), idValues(m_recent.ids()));
  if (result == PersistenceMutation::Applied)
    Q_EMIT recentChanged();
  return result;
}

} // namespace QindaQt::Shell::Launcher
