// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "authentication_controller.h"
#include "input_activity_filter.h"
#include "native_authentication_ports.h"
#include "output_window_supervisor.h"
#include "saver_presentation.h"
#include <QGuiApplication>
#include <QHash>
#include <QPointer>
#include <QQuickView>
#include <QTimer>
#include <memory>
namespace QindaQt::LockGreeter {
// Owns one ext-session-lock-role QtQuick surface per real output, all on the
// GUI thread. The controller borrows the first role's integration-owned client.
// Output removal deletes that view BEFORE Qt's ordinary primary migration.
//
// AGENT-GUARD: nothing here ends the process. An output whose window cannot be
// created (role refused, QML failed, no output yet) is retried with bounded
// backoff by the supervisor while the compositor keeps it protected black; the
// other outputs keep their windows. The locker exits only when the lock itself
// ends (unlock processed or finished by the compositor), and says why in the
// journal (ADR-0382).
class NativeWindows final : public QObject, private OutputWindowFactory, private RetryScheduler {
  Q_OBJECT
public:
  NativeWindows(QGuiApplication &app, LockWorkerClient::WorkerProcess &worker,
                SaverPresentation &saver, QString user);
  ~NativeWindows() override;
  // Starts tracking outputs and creates the windows of those present now.
  bool start();
  // Null until the first window obtained the protocol client.
  AuthenticationController *controller() const { return m_controller.get(); }
  std::size_t windowCount() const { return m_supervisor.windowCount(); }
private:
  Result create(OutputKey output) override;
  void destroy(OutputKey output) override;
  void schedule(OutputKey output, std::chrono::milliseconds delay) override;
  void cancel(OutputKey output) override;
  void track(QScreen *screen);
  void untrack(QScreen *screen);
  void ensureController(LockProtocol::ProtocolClient &client);
  QGuiApplication &m_app;
  LockWorkerClient::WorkerProcess &m_worker;
  SaverPresentation &m_saver;
  QString m_user;
  QHash<OutputKey, QPointer<QScreen>> m_screens;
  QHash<OutputKey, QQuickView *> m_windows;
  QHash<OutputKey, QTimer *> m_timers;
  InputActivityFilter *m_input;
  std::unique_ptr<WorkerAttemptRunner> m_runner;
  std::unique_ptr<ProtocolSession> m_session;
  std::unique_ptr<AuthenticationController> m_controller;
  OutputWindowSupervisor m_supervisor;
};
} // namespace QindaQt::LockGreeter
