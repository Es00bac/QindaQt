// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "worker_wire.h"
#include <QObject>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#include <memory>
namespace QindaQt::LockWorkerClient {
using LockAuthentication::AttemptToken;
using LockAuthentication::Outcome;
using LockAuthentication::WireFrame;
// GUI-thread owned disposable PAM process/private socket. Native controller
// borrows this object, invalidates coordinator tokens BEFORE cancel, and alone
// consumes completed(). Nothing here is a QML object or public service.
// start is admission only; prompts carry no password/result authority. Whole
// attempt deadline kills only our owned child even if a PAM module blocks.
class WorkerProcess final : public QObject {
  Q_OBJECT
public:
  explicit WorkerProcess(QObject *parent = nullptr);
#if defined(QINDAQT_PRIVATE_WORKER_CLIENT_FIXTURE)
  // Compiled solely into non-installed tests; production exposes no path/args.
  WorkerProcess(QString program, QString configuration, int deadlineMs);
  qint64 testProcessId() const { return m_process.processId(); }
#endif
  ~WorkerProcess() override;
  bool start(AttemptToken token);
  bool respond(QString response);
  void cancel();
  bool active() const { return m_active; }
Q_SIGNALS:
  void prompt(LockAuthentication::MessageKind kind, QString text);
  void completed(AttemptToken token, Outcome outcome);
private:
  bool send(WireFrame frame);
  bool flush();
  void drain();
  void consume(WireFrame frame);
  void abort(Outcome reason);
  void finish(int code, QProcess::ExitStatus status);
  void closeChannel();
  void complete(Outcome outcome);
  QProcess m_process;
  QTimer m_deadline;
  std::unique_ptr<QSocketNotifier> m_reader, m_writer;
  int m_fd = -1, m_childFd = -1, m_timeoutMs = 90000;
  QString m_program;
  QStringList m_arguments;
  AttemptToken m_token;
  std::string m_incoming, m_outgoing;
  std::size_t m_written = 0, m_messages = 0;
  std::optional<Outcome> m_result, m_failure;
  bool m_active = false, m_waiting = false;
#if defined(QINDAQT_PRIVATE_WORKER_CLIENT_FIXTURE)
  bool m_fixture = false;
#endif
};
}
