// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "authentication_ports.h"
#include "protocol_client.h"
#include "worker_process.h"
namespace QindaQt::LockGreeter {
// Forwarding adapters over the real borrowed collaborators. They add no policy:
// the protocol client stays owned by Qt's shell integration and the worker by
// the composition root, and both must outlive these adapters.
class ProtocolSession final : public LockSessionPort {
public:
  explicit ProtocolSession(LockProtocol::ProtocolClient &client, QObject *parent = nullptr);
  bool isLocked() const override { return m_client.isLocked(); }
  bool isAvailable() const override { return m_client.available(); }
  QString keyboardLayout() const override { return m_client.keyboardLayout(); }
  bool unlockAuthenticated() override { return m_client.unlockAuthenticated(); }

private:
  LockProtocol::ProtocolClient &m_client;
};
class WorkerAttemptRunner final : public AttemptRunnerPort {
public:
  explicit WorkerAttemptRunner(LockWorkerClient::WorkerProcess &worker, QObject *parent = nullptr);
  bool start(LockAuthentication::AttemptToken token) override { return m_worker.start(token); }
  bool respond(QString response) override { return m_worker.respond(std::move(response)); }
  void cancel() override { m_worker.cancel(); }
  bool active() const override { return m_worker.active(); }

private:
  LockWorkerClient::WorkerProcess &m_worker;
};
} // namespace QindaQt::LockGreeter
