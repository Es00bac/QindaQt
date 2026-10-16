// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "attempt_coordinator.h"
#include "authentication_ports.h"
#include <QObject>
#include <QString>
namespace QindaQt::LockGreeter {
// GUI-thread controller borrowing a lock-session port and an attempt-runner
// port for its whole lifetime. QML sees only this prompt interface, never
// either collaborator. Locked epochs come exclusively from native protocol
// events; completion comes exclusively from the owned worker's validated
// private channel (ADR-0299).
//
// Lifecycle (ADR-0382). The screen locks into Idle: the credential field is
// visible, enabled and focused, and NO PAM conversation exists. A person's first
// key press, click or touch (noteInput) or an explicit submit starts one:
//   Idle -> Starting -> Prompting -> Verifying -> Idle
// Every ending other than an authenticated unlock returns to Idle by itself
// with the field intact; the next input starts a fresh conversation. Enter
// (submit) therefore always does something visible and the prompt can never go
// dead. Nothing starts an attempt without input, so an idle lock cannot add to
// pam_faillock.
class AuthenticationController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool locked READ locked NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool waiting READ waiting NOTIFY changed)
  Q_PROPERTY(bool verifying READ verifying NOTIFY changed)
  Q_PROPERTY(bool secret READ secret NOTIFY changed)
  Q_PROPERTY(QString promptText READ promptText NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString user READ user CONSTANT)
  Q_PROPERTY(QString layout READ layout NOTIFY changed)
public:
  AuthenticationController(LockSessionPort &session, AttemptRunnerPort &runner,
                           QString user, QObject *parent = nullptr);
  ~AuthenticationController() override;
  bool locked() const { return m_entered && m_session.isLocked(); }
  // An attempt exists (or its child is still being reaped).
  bool busy() const { return m_phase != Phase::Idle || m_runner.active(); }
  // A PAM prompt is on screen waiting for a response.
  bool waiting() const { return m_waiting; }
  // A response was sent and PAM has not answered yet.
  bool verifying() const { return m_phase == Phase::Verifying; }
  bool secret() const { return m_secret; }
  QString promptText() const { return m_prompt; }
  QString status() const { return m_status; }
  QString user() const { return m_user; }
  QString layout() const { return m_session.keyboardLayout(); }

  // First key press, click or touch (never Escape). Starts an attempt when the
  // greeter is ready; a no-op while one exists or after an attempt that died
  // before offering any prompt (only submit/begin retry those).
  Q_INVOKABLE void noteInput();
  // Enter / Unlock / touch-keyboard Enter. Sends the response to the pending
  // prompt, or starts an attempt and holds a non-empty response for its first
  // secret prompt. An empty password is never sent: it cannot authenticate
  // (PAM_DISALLOW_NULL_AUTHTOK) and would only count as a failed login.
  Q_INVOKABLE void submit(QString response);
  // Answers the pending prompt only; ignored (and wiped) when none is pending.
  Q_INVOKABLE void respond(QString response);
  // Explicit user intent to (re)start authentication without a response.
  Q_INVOKABLE void begin();
  // Escape / Cancel: abandon the attempt and clear the field.
  Q_INVOKABLE void cancel();
Q_SIGNALS:
  void changed();
  void clearFields();
  void finished();
private:
  enum class Phase { Idle, Starting, Prompting, Verifying };
  void enter();
  void startAttempt();
  void resumePendingStart();
  void sendResponse(QString response);
  void onPrompt(LockAuthentication::MessageKind kind, const QString &text);
  void onRejected();
  void complete(LockAuthentication::AttemptToken token, LockAuthentication::Outcome outcome);
  void resetAttempt();
  void queue(QString response);
  void discardQueued();
  QString statusFor(LockAuthentication::Outcome outcome, bool prompted) const;
  LockSessionPort &m_session;
  AttemptRunnerPort &m_runner;
  LockAuthentication::AttemptCoordinator m_attempts;
  QString m_user, m_prompt, m_status, m_queued;
  Phase m_phase = Phase::Idle;
  bool m_waiting = false, m_secret = true, m_entered = false;
  // Input seen before the compositor confirmed the lock: starts the first attempt.
  bool m_inputBeforeLock = false;
  // false after an attempt that ended before any prompt (account locked out,
  // deny-only policy, worker could not start): plain input stops respawning
  // workers; submit/begin/cancel re-arm it.
  bool m_inputStartsAttempt = true;
  // A start was requested while the previous child was still being reaped.
  bool m_startPending = false;
  bool m_prompted = false, m_responded = false;
};
} // namespace QindaQt::LockGreeter
