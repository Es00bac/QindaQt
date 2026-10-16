// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "attempt_coordinator.h"
#include <QObject>
#include <QString>
namespace QindaQt::LockGreeter {
// The controller's two collaborators, declared by their consumer so it needs
// neither a Wayland connection nor a real PAM child to be exercised. Production
// adapters live in native_authentication_ports.h and only forward.
//
// Threading: GUI thread only. Lifetime: each port outlives the controller that
// borrows it. Error behavior: no method throws; failures are reported through
// return values and the completion signal.

// Lock state and the standard unlock request, from the native protocol client.
class LockSessionPort : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  virtual bool isLocked() const = 0;
  // false once the compositor finished the lock (or it was never granted): the
  // lock can then never be held or unlocked and the locker has nothing to do.
  virtual bool isAvailable() const = 0;
  virtual QString keyboardLayout() const = 0;
  // AGENT-CONTRACT: the only way to ask the compositor for the standard unlock.
  // Only the controller may call it, after consuming current epoch/request
  // approval from the attempt coordinator (ADR-0299).
  virtual bool unlockAuthenticated() = 0;
Q_SIGNALS:
  void locked();         // compositor confirmed the lock
  void rejected();       // compositor finished the lock without our unlock
  void unlockProcessed(); // server processed our unlock request
  void keyboardChanged();
};

// One disposable PAM attempt at a time. completed() is the sole carrier of an
// authentication result and always names the token it answers.
class AttemptRunnerPort : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  virtual bool start(LockAuthentication::AttemptToken token) = 0;
  virtual bool respond(QString response) = 0;
  virtual void cancel() = 0;
  virtual bool active() const = 0;
Q_SIGNALS:
  void prompt(LockAuthentication::MessageKind kind, QString text);
  void completed(LockAuthentication::AttemptToken token, LockAuthentication::Outcome outcome);
};
} // namespace QindaQt::LockGreeter
