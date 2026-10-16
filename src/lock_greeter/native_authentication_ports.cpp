// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_authentication_ports.h"
namespace QindaQt::LockGreeter {
ProtocolSession::ProtocolSession(LockProtocol::ProtocolClient &client, QObject *parent)
    : LockSessionPort(parent), m_client(client) {
  using Client = LockProtocol::ProtocolClient;
  connect(&client, &Client::locked, this, &LockSessionPort::locked);
  connect(&client, &Client::rejected, this, &LockSessionPort::rejected);
  connect(&client, &Client::unlockProcessed, this, &LockSessionPort::unlockProcessed);
  connect(&client, &Client::keyboardChanged, this, &LockSessionPort::keyboardChanged);
}
WorkerAttemptRunner::WorkerAttemptRunner(LockWorkerClient::WorkerProcess &worker, QObject *parent)
    : AttemptRunnerPort(parent), m_worker(worker) {
  using Worker = LockWorkerClient::WorkerProcess;
  connect(&worker, &Worker::prompt, this, &AttemptRunnerPort::prompt);
  connect(&worker, &Worker::completed, this, &AttemptRunnerPort::completed);
}
} // namespace QindaQt::LockGreeter
