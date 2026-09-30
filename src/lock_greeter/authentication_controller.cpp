// SPDX-License-Identifier: GPL-3.0-or-later
#include "authentication_controller.h"
namespace QindaQt::LockGreeter {
using namespace LockAuthentication;
AuthenticationController::AuthenticationController(LockProtocol::ProtocolClient &protocol,
    LockWorkerClient::WorkerProcess &worker, QString user, QObject *parent)
    : QObject(parent), m_protocol(protocol), m_worker(worker), m_user(std::move(user)) {
  connect(&protocol, &LockProtocol::ProtocolClient::locked, this, &AuthenticationController::enter);
  connect(&protocol, &LockProtocol::ProtocolClient::keyboardChanged, this, &AuthenticationController::changed);
  connect(&protocol, &LockProtocol::ProtocolClient::rejected, this, [this] {
    m_attempts.leaveLockedSession(); m_worker.cancel(); Q_EMIT clearFields(); Q_EMIT finished();
  });
  connect(&protocol, &LockProtocol::ProtocolClient::unlockProcessed, this, &AuthenticationController::finished);
  connect(&worker, &LockWorkerClient::WorkerProcess::completed, this, &AuthenticationController::complete);
  connect(&worker, &LockWorkerClient::WorkerProcess::prompt, this, [this](MessageKind kind, const QString &text) {
    if (!m_attempts.busy()) return;
    m_waiting = kind == MessageKind::Secret || kind == MessageKind::Visible;
    if (m_waiting) {
      m_secret = kind == MessageKind::Secret; m_prompt = text;
      Q_EMIT clearFields();
    } else m_status = text;
    Q_EMIT changed();
  });
  if (protocol.isLocked()) enter();
}
AuthenticationController::~AuthenticationController() {
  // AGENT-GUARD: invalidate first; killing the owned child may synchronously
  // publish a cancellation/result and must never consume an old approval.
  m_attempts.leaveLockedSession(); disconnect(&m_worker, nullptr, this, nullptr); m_worker.cancel();
}
void AuthenticationController::enter() {
  if (m_entered || !m_protocol.isLocked()) return;
  m_entered = true; m_attempts.enterLockedSession(); begin();
}
void AuthenticationController::begin() {
  if (!m_protocol.isLocked() || m_worker.active()) return;
  const auto token = m_attempts.begin(); if (!token) return;
  m_waiting = false; m_status.clear(); m_prompt.clear(); Q_EMIT clearFields();
  if (!m_worker.start(*token)) complete(*token, Outcome::Unavailable);
  Q_EMIT changed();
}
void AuthenticationController::respond(QString response) {
  if (!m_attempts.busy() || !m_waiting) { response.fill(QChar()); return; }
  m_waiting = false; Q_EMIT clearFields(); Q_EMIT changed();
  const bool accepted = m_worker.respond(response); response.fill(QChar());
  if (!accepted) m_worker.cancel(); // Never reinterpret a transport failure as approval.
}
void AuthenticationController::cancel() {
  m_attempts.cancel(); m_waiting = false; m_prompt.clear(); m_status.clear();
  Q_EMIT clearFields(); m_worker.cancel(); Q_EMIT changed();
}
void AuthenticationController::complete(AttemptToken token, Outcome result) {
  const auto action = m_attempts.complete(token, result);
  if (action == Completion::Ignored) { Q_EMIT changed(); return; }
  m_waiting = false; m_prompt.clear(); Q_EMIT clearFields();
  if (action == Completion::Unlock) {
    // AGENT-CONTRACT: this is the sole native call site for standard unlock.
    // No QObject method accepts an authentication result from QML or D-Bus.
    if (!m_protocol.unlockAuthenticated()) m_status = tr("Unlock unavailable.");
  } else {
    switch (result) {
    case Outcome::Denied: m_status = tr("Authentication failed. Try again."); break;
    case Outcome::AccountDenied: m_status = tr("This account cannot unlock this session."); break;
    case Outcome::Cancelled: m_status.clear(); break;
    default: m_status = tr("Authentication unavailable. Try again."); break;
    }
  }
  Q_EMIT changed();
}
}
