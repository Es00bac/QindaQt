// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/session_lock_state/native_lock_state_monitor.h"
#include "qindaqt/services/session_lock_state/native_lock_transport.h"
#include <QRegularExpression>
#include <QThread>
#include <limits>
namespace QindaQt::Services::SessionLockState {
NativeLockStateMonitor::NativeLockStateMonitor(NativeLockTransport &transport,
                                               NativeLockAdmission admission,
                                               QObject *parent)
    : NativeLockStateMonitor(transport, std::move(admission),
                             NativeLockMonitorRecovery{}, parent) {}
NativeLockStateMonitor::NativeLockStateMonitor(NativeLockTransport &transport,
                                               NativeLockAdmission admission,
                                               NativeLockMonitorRecovery recovery,
                                               QObject *parent)
    : QObject(parent), m_transport(transport),
      m_admission(std::move(admission)),
      m_recovery(recovery.policy, recovery.timers) {
  connect(&m_recovery, &Platform::Recovery::RecoveryBackoff::retry, this,
          &NativeLockStateMonitor::recover);
  connect(&transport, &NativeLockTransport::lost, this, [this] {
    m_started = false;
    m_recovery.reset();
    invalidate();
  });
  connect(&transport, &NativeLockTransport::ownerChanged, this, [this] {
    m_recovery.reset();
    probe();
  });
  connect(&transport, &NativeLockTransport::ownerResolved, this,
          [this](quint64 generation, const QString &owner) {
            if (!m_started || generation != m_generation)
              return;
            static const QRegularExpression unique(
                QStringLiteral(R"(^:[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)+$)"));
            if (owner.size() > 255 || !unique.match(owner).hasMatch()) {
              // No compositor yet: ownerChanged probes as soon as one
              // appears; the bounded retry covers a lost owner signal.
              invalidate();
              retryLater();
              return;
            }
            m_owner = owner;
            m_transport.requestPid(generation, owner);
          });
  connect(&transport, &NativeLockTransport::pidResolved, this,
          [this](quint64 generation, const QString &owner, quint64 pid) {
            if (!m_started || generation != m_generation || owner != m_owner)
              return;
            if (!pid || pid > std::numeric_limits<quint32>::max() ||
                !m_admission(owner, pid)) {
              invalidate();
              retryLater();
              return;
            }
            m_pid = pid;
            m_authenticated = true;
            // Install both native signals before the first snapshot. A signal
            // racing a GetAll invalidates its serial; late unlocked replies
            // cannot disclose.
            if (!m_transport.subscribe(owner)) {
              invalidate();
              retryLater();
              return;
            }
            query();
          });
  connect(&transport, &NativeLockTransport::stateInvalidated, this,
          [this](const QString &owner) {
            if (!m_started || owner != m_owner || !m_authenticated)
              return;
            publish(LockState::Unknown, false);
            query();
          });
  connect(&transport, &NativeLockTransport::stateResolved, this,
          [this](quint64 generation, quint64 serial, const QString &owner,
                 bool locked, bool protectedPresentation) {
            if (!m_started || generation != m_generation ||
                serial != m_serial || owner != m_owner)
              return;
            if (!admitted()) {
              invalidate();
              retryLater();
              return;
            }
            if (!locked && protectedPresentation) {
              // A transition snapshot; the next change signal re-queries.
              publish(LockState::Unknown, false);
              return;
            }
            m_recovery.reset();
            publish(!locked                 ? LockState::Unlocked
                    : protectedPresentation ? LockState::Locked
                                            : LockState::Locking,
                    protectedPresentation);
          });
  connect(&transport, &NativeLockTransport::failed, this,
          [this](quint64 generation, quint64 serial, const QString &owner,
                 const QString &) {
            if (!m_started || generation != m_generation ||
                (serial && serial != m_serial) ||
                (!owner.isEmpty() && owner != m_owner))
              return;
            // AGENT-GUARD: a failure (including a timeout against a slow
            // compositor or bus) is never permanent and never supplies state.
            if (!serial)
              invalidate();
            else {
              ++m_serial;
              publish(LockState::Unknown, false);
            }
            retryLater();
          });
}
NativeLockStateMonitor::~NativeLockStateMonitor() { stop(); }
bool NativeLockStateMonitor::start(QString *error) {
  if (m_started)
    return true;
  if (!m_admission || thread() != m_transport.thread() ||
      QThread::currentThread() != thread()) {
    if (error)
      *error = QStringLiteral(
          "native lock admission and same-thread transport are required");
    return false;
  }
  if (!m_transport.start(error))
    return false;
  m_started = true;
  m_recovery.reset();
  probe();
  return true;
}
void NativeLockStateMonitor::stop() {
  m_started = false;
  m_recovery.reset();
  invalidate();
  m_transport.stop();
}
void NativeLockStateMonitor::refresh() {
  m_recovery.reset();
  if (m_started)
    probe();
  else
    invalidate();
}
void NativeLockStateMonitor::probe() {
  if (!m_started)
    return;
  invalidate();
  m_transport.requestOwner(m_generation);
}
void NativeLockStateMonitor::query() {
  if (!m_started || !admitted()) {
    invalidate();
    retryLater();
    return;
  }
  m_transport.requestState(m_generation, ++m_serial, m_owner);
}
void NativeLockStateMonitor::retryLater() {
  if (m_started)
    m_recovery.schedule();
}
void NativeLockStateMonitor::recover() {
  if (!m_started)
    return;
  // A still-admitted subscribed owner only needs a fresh snapshot; anything
  // else re-resolves the owner from scratch.
  if (admitted())
    query();
  else
    probe();
}
void NativeLockStateMonitor::invalidate() {
  ++m_generation;
  ++m_serial;
  m_authenticated = false;
  m_owner.clear();
  m_pid = 0;
  m_transport.unsubscribe();
  publish(LockState::Unknown, false);
}
bool NativeLockStateMonitor::admitted() const {
  return m_started && m_authenticated && !m_owner.isEmpty() && m_pid &&
         m_admission && m_admission(m_owner, m_pid);
}
LockState NativeLockStateMonitor::state() const {
  return admitted() ? m_state : LockState::Unknown;
}
bool NativeLockStateMonitor::presentationProtected() const {
  return admitted() && m_state == LockState::Locked && m_protected;
}
void NativeLockStateMonitor::publish(LockState next,
                                     bool protectedPresentation) {
  // Admission is checked at disclosure time as well as before each query.
  // A revoked attachment cannot consume a reply already queued in Qt.
  if ((next != LockState::Unknown || protectedPresentation) && !admitted()) {
    next = LockState::Unknown;
    protectedPresentation = false;
  }
  const bool contentBefore = m_state == LockState::Unlocked;
  const bool changed = m_state != next,
             protection = m_protected != protectedPresentation;
  m_state = next;
  m_protected = protectedPresentation;
  // A direct observer can revoke admission while handling the first signal.
  // Recheck each subsequent disclosure rather than replaying the snapshot.
  if (changed)
    Q_EMIT stateChanged(state());
  const bool contentNow = contentMayBeShown();
  if (contentBefore != contentNow)
    Q_EMIT contentMayBeShownChanged(contentNow);
  if (protection)
    Q_EMIT protectionChanged(presentationProtected());
}
} // namespace QindaQt::Services::SessionLockState
