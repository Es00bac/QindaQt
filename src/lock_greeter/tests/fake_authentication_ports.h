// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Fake collaborators for the greeter's behavior tests. A real WorkerProcess
// reports its deadline as completed(token, Outcome::TimedOut) and reaps its child
// asynchronously; these fakes model exactly that surface, nothing more, so tests
// are deterministic (no process, no timer).
#include "authentication_ports.h"
#include <vector>
namespace QindaQt::LockGreeter::TestSupport {
using namespace LockAuthentication;
class FakeSession final : public LockSessionPort {
public:
  bool isLocked() const override { return m_locked; }
  bool isAvailable() const override { return available; }
  QString keyboardLayout() const override { return QStringLiteral("English (US)"); }
  bool unlockAuthenticated() override {
    ++unlocks;
    return unlockAccepted;
  }
  void lock() {
    m_locked = true;
    Q_EMIT locked();
  }
  void reject() { Q_EMIT rejected(); }
  void processUnlock() { Q_EMIT unlockProcessed(); }
  bool unlockAccepted = true;
  bool available = true;
  int unlocks = 0;

private:
  bool m_locked = false;
};

class FakeRunner final : public AttemptRunnerPort {
public:
  bool start(AttemptToken token) override {
    ++starts;
    tokens.push_back(token);
    current = token;
    if (startFails) return false;
    m_active = true;
    return true;
  }
  bool respond(QString response) override {
    responses.push_back(response);
    return respondAccepted;
  }
  // Like WorkerProcess, cancel only asks the child to die: the process is reaped
  // (active() turns false and completed() fires) later, when a test says so.
  void cancel() override { ++cancels; }
  bool active() const override { return m_active; }
  // Test drivers (the signal names are taken by the port).
  void offer(MessageKind kind, const QString &text) { Q_EMIT prompt(kind, text); }
  void finish(Outcome outcome, AttemptToken token) {
    m_active = false;
    Q_EMIT completed(token, outcome);
  }
  void finish(Outcome outcome) { finish(outcome, current); }
  bool startFails = false, respondAccepted = true;
  int starts = 0, cancels = 0;
  AttemptToken current;
  std::vector<AttemptToken> tokens;
  std::vector<QString> responses;

private:
  bool m_active = false;
};
} // namespace QindaQt::LockGreeter::TestSupport
