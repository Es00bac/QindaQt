// SPDX-License-Identifier: GPL-3.0-or-later
#include "authentication_controller.h"
#include "greeter_log.h"
namespace QindaQt::LockGreeter {
using namespace LockAuthentication;
namespace {
void wipe(QString &secret) {
  secret.fill(QChar());
  secret.clear();
}
const char *outcomeName(Outcome outcome) {
  switch (outcome) {
  case Outcome::Authenticated: return "authenticated";
  case Outcome::Denied: return "denied";
  case Outcome::AccountDenied: return "account-denied";
  case Outcome::Cancelled: return "cancelled";
  case Outcome::Unavailable: return "unavailable";
  case Outcome::TimedOut: return "timed-out";
  }
  return "unknown";
}
} // namespace
AuthenticationController::AuthenticationController(LockSessionPort &session, AttemptRunnerPort &runner,
                                                   QString user, QObject *parent)
    : QObject(parent), m_session(session), m_runner(runner), m_user(std::move(user)) {
  connect(&session, &LockSessionPort::locked, this, &AuthenticationController::enter);
  connect(&session, &LockSessionPort::keyboardChanged, this, &AuthenticationController::changed);
  connect(&session, &LockSessionPort::rejected, this, &AuthenticationController::onRejected);
  connect(&session, &LockSessionPort::unlockProcessed, this, [this] {
    qCInfo(lcGreeter, "unlock processed by the compositor; the locker is finished");
    Q_EMIT finished();
  });
  connect(&runner, &AttemptRunnerPort::completed, this, &AuthenticationController::complete);
  connect(&runner, &AttemptRunnerPort::prompt, this, &AuthenticationController::onPrompt);
  if (session.isLocked()) enter();
  // The compositor may have finished the lock before anyone was listening (the
  // event arrives during Qt's platform start-up). That is an exit, not a wait:
  // a locker that lingers looks alive to the compositor's supervision.
  if (!session.isAvailable() && !session.isLocked())
    QMetaObject::invokeMethod(this, &AuthenticationController::onRejected, Qt::QueuedConnection);
}
AuthenticationController::~AuthenticationController() {
  // AGENT-GUARD: invalidate first; killing the owned child may synchronously
  // publish a cancellation/result and must never consume an old approval.
  m_attempts.leaveLockedSession();
  disconnect(&m_runner, nullptr, this, nullptr);
  m_runner.cancel();
  discardQueued();
}
void AuthenticationController::enter() {
  if (m_entered || !m_session.isLocked()) return;
  m_entered = true;
  m_attempts.enterLockedSession();
  m_inputStartsAttempt = true;
  // AGENT-GUARD: no attempt starts here. A PAM conversation exists only because
  // a person pressed a key, clicked or touched; starting one at lock time left
  // a 90 s deadline running on an unattended screen and killed the prompt
  // afterwards (ADR-0382). The field is already usable in Idle.
  const bool inputArrivedFirst = m_inputBeforeLock;
  m_inputBeforeLock = false;
  if (inputArrivedFirst) startAttempt();
  Q_EMIT changed();
}
void AuthenticationController::noteInput() {
  if (!m_entered) {
    m_inputBeforeLock = true;
    return;
  }
  if (m_phase != Phase::Idle || !m_inputStartsAttempt) return;
  startAttempt();
}
void AuthenticationController::begin() {
  m_inputStartsAttempt = true;
  startAttempt();
}
void AuthenticationController::startAttempt() {
  if (m_phase != Phase::Idle || !m_session.isLocked()) return;
  if (m_runner.active()) {
    // The previous child is still being reaped; start as soon as it is gone.
    m_startPending = true;
    return;
  }
  const auto token = m_attempts.begin();
  if (!token) return;
  m_startPending = false;
  // AGENT-GUARD: resetAttempt() keeps the credential field. The key press that
  // started this attempt is already in it; clearing here would drop the first
  // character of every password.
  resetAttempt();
  m_phase = Phase::Starting;
  qCInfo(lcGreeter, "starting an authentication attempt on user input");
  if (!m_runner.start(*token)) {
    complete(*token, Outcome::Unavailable);
    return;
  }
  Q_EMIT changed();
}
void AuthenticationController::resumePendingStart() {
  if (!m_startPending || m_runner.active()) return;
  m_startPending = false;
  startAttempt();
}
void AuthenticationController::submit(QString response) {
  if (!m_session.isLocked()) {
    wipe(response);
    return;
  }
  switch (m_phase) {
  case Phase::Prompting:
    if (m_secret && response.isEmpty()) {
      m_status = tr("Type your password, then press Enter.");
      Q_EMIT changed();
      return;
    }
    sendResponse(std::move(response));
    return;
  case Phase::Idle:
    m_inputStartsAttempt = true;
    queue(std::move(response));
    startAttempt();
    if (m_phase == Phase::Idle && !m_startPending) discardQueued();
    return;
  case Phase::Starting:
    queue(std::move(response));
    return;
  case Phase::Verifying:
    // The previous response is still being verified. Leave the text in the
    // field (no clearFields): it is the next attempt's input, typed ahead.
    wipe(response);
    return;
  }
}
void AuthenticationController::respond(QString response) {
  if (m_phase != Phase::Prompting) {
    wipe(response);
    return;
  }
  sendResponse(std::move(response));
}
void AuthenticationController::sendResponse(QString response) {
  m_waiting = false;
  m_phase = Phase::Verifying;
  m_responded = true;
  m_status = tr("Checking…");
  Q_EMIT clearFields();
  Q_EMIT changed();
  const bool accepted = m_runner.respond(response);
  response.fill(QChar());
  if (!accepted) m_runner.cancel(); // Never reinterpret a transport failure as approval.
}
void AuthenticationController::onPrompt(MessageKind kind, const QString &text) {
  if (m_phase == Phase::Idle || !m_attempts.busy()) return; // Late frame of a cancelled attempt.
  if (kind != MessageKind::Secret && kind != MessageKind::Visible) {
    m_status = text;
    Q_EMIT changed();
    return;
  }
  const bool isSecret = kind == MessageKind::Secret;
  // Characters typed before the very first secret prompt belong to it; a visible
  // or later prompt cannot be assumed to want them, so they are discarded.
  const bool keepTypeAhead = !m_prompted && isSecret && !m_responded;
  if (m_phase == Phase::Verifying) m_status.clear();
  m_prompted = true;
  m_inputStartsAttempt = true; // A real prompt proves the conversation works.
  m_waiting = true;
  m_secret = isSecret;
  m_prompt = text;
  m_phase = Phase::Prompting;
  if (!keepTypeAhead) {
    discardQueued();
    Q_EMIT clearFields();
  }
  Q_EMIT changed();
  if (!m_queued.isEmpty()) {
    QString held = std::move(m_queued);
    m_queued = QString();
    sendResponse(std::move(held));
  }
}
void AuthenticationController::cancel() {
  // AGENT-GUARD: invalidate the coordinator token before killing the child; the
  // runner may synchronously publish a completion that must not be consumed.
  m_attempts.cancel();
  m_runner.cancel();
  discardQueued();
  resetAttempt();
  m_phase = Phase::Idle;
  m_startPending = false;
  m_inputStartsAttempt = true;
  Q_EMIT clearFields();
  Q_EMIT changed();
}
void AuthenticationController::onRejected() {
  qCWarning(lcGreeter, "the compositor finished the session lock; the locker exits without unlocking");
  m_attempts.leaveLockedSession();
  m_runner.cancel();
  discardQueued();
  resetAttempt();
  m_phase = Phase::Idle;
  m_startPending = false;
  Q_EMIT clearFields();
  Q_EMIT finished();
}
void AuthenticationController::complete(AttemptToken token, Outcome outcome) {
  const auto action = m_attempts.complete(token, outcome);
  if (action == Completion::Ignored) {
    resumePendingStart();
    Q_EMIT changed();
    return;
  }
  const bool prompted = m_prompted;
  m_phase = Phase::Idle;
  m_waiting = false;
  m_prompt.clear();
  m_prompted = m_responded = false;
  discardQueued();
  if (action == Completion::Unlock) {
    m_status.clear();
    Q_EMIT clearFields();
    qCInfo(lcGreeter, "authentication approved; requesting the standard unlock");
    // AGENT-CONTRACT: this is the sole native call site for standard unlock.
    // No QObject method accepts an authentication result from QML or D-Bus.
    if (!m_session.unlockAuthenticated()) {
      m_status = tr("Unlock unavailable.");
      qCWarning(lcGreeter, "the compositor refused the standard unlock request");
    }
  } else {
    // An attempt that died before any prompt (locked-out account, deny-only
    // policy, worker failed to start) would die the same way on every key
    // press; only an explicit submit retries it.
    m_inputStartsAttempt = prompted;
    m_status = statusFor(outcome, prompted);
    // A timed-out attempt was abandoned: do not keep a partial password around.
    if (outcome == Outcome::TimedOut) Q_EMIT clearFields();
    qCInfo(lcGreeter, "attempt ended without unlock: %s%s", outcomeName(outcome),
           prompted ? "" : " (before any prompt)");
  }
  resumePendingStart();
  Q_EMIT changed();
}
void AuthenticationController::resetAttempt() {
  // AGENT-GUARD: deliberately leaves m_queued alone. submit() queues a response
  // and then starts the attempt that will consume it; endings call
  // discardQueued() themselves.
  m_waiting = false;
  m_secret = true;
  m_prompted = m_responded = false;
  m_prompt.clear();
  m_status.clear();
}
void AuthenticationController::queue(QString response) {
  discardQueued();
  if (response.isEmpty()) return;
  m_queued = std::move(response);
  Q_EMIT clearFields(); // The text now lives here until the first prompt takes it.
}
void AuthenticationController::discardQueued() { wipe(m_queued); }
QString AuthenticationController::statusFor(Outcome outcome, bool prompted) const {
  switch (outcome) {
  case Outcome::Denied:
    return prompted ? tr("Authentication failed. Try again.")
                    : tr("Authentication was refused before asking for a password. "
                         "The account may be temporarily locked.");
  case Outcome::AccountDenied: return tr("This account cannot unlock this session.");
  case Outcome::TimedOut: return tr("Timed out. Start typing to try again.");
  case Outcome::Cancelled:
  case Outcome::Authenticated: return {};
  case Outcome::Unavailable: break;
  }
  return tr("Authentication unavailable. Try again.");
}
} // namespace QindaQt::LockGreeter
