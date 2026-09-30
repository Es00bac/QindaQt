// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "attempt_coordinator.h"
#include "protocol_client.h"
#include "worker_process.h"
#include <QObject>
namespace QindaQt::LockGreeter {
// GUI-thread controller borrows protocol and worker throughout its lifetime.
// QML sees only this prompt interface, never either authority collaborator.
// Locked epochs come exclusively from native protocol events; completion comes
// exclusively from our owned worker's validated private channel.
class AuthenticationController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool waiting READ waiting NOTIFY changed)
  Q_PROPERTY(bool secret READ secret NOTIFY changed)
  Q_PROPERTY(QString promptText READ promptText NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString user READ user CONSTANT)
  Q_PROPERTY(QString layout READ layout NOTIFY changed)
public:
  AuthenticationController(LockProtocol::ProtocolClient &protocol,
                           LockWorkerClient::WorkerProcess &worker,
                           QString user, QObject *parent = nullptr);
  ~AuthenticationController() override;
  bool busy() const { return m_attempts.busy() || m_worker.active(); }
  bool waiting() const { return m_waiting; }
  bool secret() const { return m_secret; }
  QString promptText() const { return m_prompt; }
  QString status() const { return m_status; }
  QString user() const { return m_user; }
  QString layout() const { return m_protocol.keyboardLayout(); }
  Q_INVOKABLE void begin();
  Q_INVOKABLE void respond(QString response);
  Q_INVOKABLE void cancel();
Q_SIGNALS:
  void changed();
  void clearFields();
  void finished();
private:
  void enter();
  void complete(LockAuthentication::AttemptToken token, LockAuthentication::Outcome result);
  LockProtocol::ProtocolClient &m_protocol;
  LockWorkerClient::WorkerProcess &m_worker;
  LockAuthentication::AttemptCoordinator m_attempts;
  QString m_user, m_prompt, m_status;
  bool m_waiting = false, m_secret = true, m_entered = false;
};
}
