// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include "qindaqt/shell/workspaces/workspace_transport.h"

#include <QVariantMap>

namespace QindaQt::Shell::Workspaces {

WorkspaceController::WorkspaceController(WorkspaceTransport *transport,
                                         WorkspaceGrants grants, QObject *parent)
    : QObject(parent), m_transport(transport), m_grants(grants)
{
  if (!m_grants.windowsRead) {
    // AGENT-GUARD: read denial withholds observation entirely; no fetch may
    // ever be issued, so the transport is never even connected here.
    m_phase = WorkspacePhase::Unavailable;
    m_phaseReason = QStringLiteral("windows-read-not-granted");
    return;
  }
  if (m_transport == nullptr) {
    m_phase = WorkspacePhase::Unavailable;
    m_phaseReason = QStringLiteral("workspace-transport-absent");
    return;
  }
  m_phaseReason = QStringLiteral("compositor-workspaces-loading");
  connect(m_transport, &WorkspaceTransport::ownerChanged, this,
          &WorkspaceController::handleOwnerChanged);
  connect(m_transport, &WorkspaceTransport::changed, this,
          &WorkspaceController::handleChanged);
  connect(m_transport, &WorkspaceTransport::snapshotReceived, this,
          &WorkspaceController::handleSnapshot);
  connect(m_transport, &WorkspaceTransport::snapshotFailed, this,
          &WorkspaceController::handleSnapshotFailed);
  connect(m_transport, &WorkspaceTransport::operationFinished, this,
          &WorkspaceController::handleOperationFinished);
}

QString WorkspaceController::phaseText() const
{
  return workspacePhaseText(m_phase);
}

QVariantList WorkspaceController::rows() const
{
  QVariantList rows;
  if (!m_snapshot) {
    return rows;
  }
  rows.reserve(m_snapshot->desktops.size());
  int index = 0;
  for (const WorkspaceRow &row : m_snapshot->desktops) {
    const bool current = row.id == m_snapshot->currentId;
    const QString name = row.name.isEmpty()
        ? QStringLiteral("Workspace %1").arg(index + 1)
        : row.name;
    rows.append(QVariantMap{
        {QStringLiteral("id"), row.id},
        {QStringLiteral("name"), name},
        {QStringLiteral("position"), row.position},
        {QStringLiteral("index"), index},
        {QStringLiteral("current"), current},
        {QStringLiteral("accessibleName"),
         current ? QStringLiteral("%1, current workspace").arg(name) : name},
        {QStringLiteral("revision"), m_revision},
    });
    ++index;
  }
  return rows;
}

int WorkspaceController::count() const noexcept
{
  return m_snapshot ? static_cast<int>(m_snapshot->desktops.size()) : 0;
}

QString WorkspaceController::currentId() const
{
  return m_snapshot ? m_snapshot->currentId : QString{};
}

QString WorkspaceController::currentName() const
{
  const int index = currentIndex();
  if (index < 0) {
    return {};
  }
  const WorkspaceRow &row = m_snapshot->desktops.at(index);
  return row.name.isEmpty() ? QStringLiteral("Workspace %1").arg(index + 1)
                            : row.name;
}

int WorkspaceController::currentIndex() const
{
  if (!m_snapshot) {
    return -1;
  }
  for (qsizetype index = 0; index < m_snapshot->desktops.size(); ++index) {
    if (m_snapshot->desktops.at(index).id == m_snapshot->currentId) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

bool WorkspaceController::showingDesktop() const noexcept
{
  return m_snapshot && m_snapshot->showingDesktop;
}

bool WorkspaceController::canSwitch() const noexcept
{
  return m_grants.windowsManage && m_phase == WorkspacePhase::Ready
      && !m_pending.has_value() && count() > 1;
}

bool WorkspaceController::canShowDesktop() const noexcept
{
  return m_grants.windowsManage && m_phase == WorkspacePhase::Ready
      && !m_pending.has_value();
}

QString WorkspaceController::accessibleName() const
{
  if (m_phase != WorkspacePhase::Ready) {
    return QStringLiteral("Workspaces");
  }
  return QStringLiteral("Workspaces, %1 of %2: %3")
      .arg(currentIndex() + 1)
      .arg(count())
      .arg(currentName());
}

QString WorkspaceController::accessibleDescription() const
{
  switch (m_phase) {
  case WorkspacePhase::Loading:
    return QStringLiteral("Workspace information is loading");
  case WorkspacePhase::Degraded:
    return QStringLiteral("Workspace information is limited: %1").arg(m_phaseReason);
  case WorkspacePhase::Unavailable:
    return QStringLiteral("Workspaces are unavailable: %1").arg(m_phaseReason);
  case WorkspacePhase::Ready:
    break;
  }
  if (!m_grants.windowsManage) {
    return QStringLiteral("Switching workspaces is not granted");
  }
  return showingDesktop() ? QStringLiteral("The desktop is shown")
                          : QStringLiteral("Activate a workspace to switch to it");
}

bool WorkspaceController::dispatchAllowed(QString *message) const
{
  if (!m_grants.windowsManage) {
    *message = QStringLiteral("Workspace control is not granted");
    return false;
  }
  if (m_phase != WorkspacePhase::Ready || !m_snapshot || m_owner.isEmpty()) {
    *message = QStringLiteral("Workspaces are not ready");
    return false;
  }
  if (m_pending) {
    *message = QStringLiteral("A workspace request is still pending");
    return false;
  }
  return true;
}

bool WorkspaceController::switchTo(const QString &desktopId, quint64 revision)
{
  QString message;
  if (!dispatchAllowed(&message)) {
    return refuse(message);
  }
  if (revision != m_revision) {
    return refuse(QStringLiteral("The workspace list changed; try again"));
  }
  bool known = false;
  for (const WorkspaceRow &row : m_snapshot->desktops) {
    known = known || row.id == desktopId;
  }
  if (!known) {
    return refuse(QStringLiteral("That workspace no longer exists"));
  }
  if (desktopId == m_snapshot->currentId) {
    clearFeedback();
    return true;
  }
  const quint64 token = m_nextToken++;
  m_pending = Pending{PendingKind::Switch, token, m_owner};
  clearFeedback();
  Q_EMIT stateChanged();
  m_transport->requestSwitch(token, m_owner, desktopId);
  return true;
}

bool WorkspaceController::switchRelative(int delta)
{
  QString message;
  if (!dispatchAllowed(&message)) {
    return refuse(message);
  }
  const int total = count();
  const int current = currentIndex();
  if (total <= 1 || current < 0) {
    return refuse(QStringLiteral("There is only one workspace"));
  }
  const int step = ((delta % total) + total) % total;
  const int target = (current + step) % total;
  return switchTo(m_snapshot->desktops.at(target).id, m_revision);
}

bool WorkspaceController::setShowingDesktop(bool showing)
{
  QString message;
  if (!dispatchAllowed(&message)) {
    return refuse(message);
  }
  if (m_snapshot->showingDesktop == showing) {
    clearFeedback();
    return true;
  }
  const quint64 token = m_nextToken++;
  m_pending = Pending{PendingKind::ShowDesktop, token, m_owner};
  clearFeedback();
  Q_EMIT stateChanged();
  m_transport->requestShowDesktop(token, m_owner, showing);
  return true;
}

bool WorkspaceController::toggleShowingDesktop()
{
  return setShowingDesktop(!showingDesktop());
}

void WorkspaceController::clearFeedback()
{
  publishFeedback({});
}

void WorkspaceController::handleOwnerChanged(const QString &owner,
                                             const QString &reasonCode)
{
  if (owner == m_owner && !owner.isEmpty()) {
    return;
  }
  // AGENT-GUARD: any owner change clears prior truth. A pending mutation
  // becomes Uncertain feedback and is never replayed against the new owner.
  m_owner = owner;
  m_snapshot.reset();
  m_fetchToken = 0;
  m_fetchDirty = false;
  if (m_pending) {
    m_pending.reset();
    publishFeedback(QStringLiteral(
        "The compositor changed before the workspace request completed"));
  }
  if (owner.isEmpty()) {
    setPhase(WorkspacePhase::Unavailable,
             reasonCode.isEmpty() ? QStringLiteral("compositor-unavailable")
                                  : reasonCode.left(Bounds::maxReasonLength));
    return;
  }
  setPhase(WorkspacePhase::Loading,
           QStringLiteral("compositor-workspaces-loading"));
  fetchSnapshot();
}

void WorkspaceController::handleChanged(const QString &owner)
{
  if (owner.isEmpty() || owner != m_owner) {
    return;
  }
  if (m_fetchToken != 0) {
    // Coalesce: one refetch after the in-flight read completes.
    m_fetchDirty = true;
    return;
  }
  fetchSnapshot();
}

void WorkspaceController::fetchSnapshot()
{
  if (m_owner.isEmpty() || m_transport == nullptr) {
    return;
  }
  m_fetchDirty = false;
  m_fetchToken = m_nextToken++;
  m_transport->requestSnapshot(m_fetchToken, m_owner);
}

void WorkspaceController::handleSnapshot(quint64 token, const QString &owner,
                                         const WorkspaceSnapshot &snapshot)
{
  if (token != m_fetchToken || owner != m_owner) {
    return; // stale or foreign reply
  }
  m_fetchToken = 0;
  const WorkspaceValidation validation = validateWorkspaceSnapshot(snapshot);
  if (!validation.ok) {
    m_snapshot.reset();
    setPhase(WorkspacePhase::Degraded, validation.reasonCode);
  } else {
    m_snapshot = validation.snapshot;
    ++m_revision;
    setPhase(WorkspacePhase::Ready, {});
  }
  if (m_fetchDirty) {
    fetchSnapshot();
  }
}

void WorkspaceController::handleSnapshotFailed(quint64 token, const QString &owner,
                                               const QString &reasonCode)
{
  if (token != m_fetchToken || owner != m_owner) {
    return;
  }
  m_fetchToken = 0;
  m_snapshot.reset();
  setPhase(WorkspacePhase::Degraded,
           reasonCode.isEmpty() ? QStringLiteral("workspace-snapshot-failed")
                                : reasonCode.left(Bounds::maxReasonLength));
  if (m_fetchDirty) {
    fetchSnapshot();
  }
}

void WorkspaceController::handleOperationFinished(quint64 token,
                                                  const QString &owner, bool ok,
                                                  const QString &reasonCode)
{
  if (!m_pending || m_pending->token != token || m_pending->owner != owner) {
    return;
  }
  const PendingKind kind = m_pending->kind;
  m_pending.reset();
  if (!ok) {
    const QString action = kind == PendingKind::Switch
        ? QStringLiteral("switch workspaces")
        : QStringLiteral("show the desktop");
    publishFeedback(QStringLiteral("Could not %1: %2")
                        .arg(action, reasonCode.left(Bounds::maxReasonLength)));
  } else {
    clearFeedback();
  }
  // AGENT-GUARD: KWin may complete a property write without emitting the
  // corresponding change signal. Re-read after every completed request so
  // the facade converges to compositor truth and never leaves stale rows or
  // show-desktop state visible after an apparently successful action.
  if (m_fetchToken != 0) {
    m_fetchDirty = true;
  } else {
    fetchSnapshot();
  }
  Q_EMIT stateChanged();
}

void WorkspaceController::setPhase(WorkspacePhase phase, const QString &reason)
{
  m_phase = phase;
  m_phaseReason = reason;
  Q_EMIT stateChanged();
}

bool WorkspaceController::refuse(const QString &message)
{
  publishFeedback(message);
  return false;
}

void WorkspaceController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::Workspaces
