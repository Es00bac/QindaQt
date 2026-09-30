// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "authentication_controller.h"
#include "saver_presentation.h"
#include <QGuiApplication>
#include <QHash>
#include <QQuickView>
#include <memory>
namespace QindaQt::LockGreeter {
// Owns one ext-session-lock-role QtQuick surface per real output, all on the
// GUI thread. The controller borrows the first role's integration-owned client.
// Output removal deletes that view BEFORE Qt's ordinary primary migration.
class NativeWindows final : public QObject {
  Q_OBJECT
public:
  NativeWindows(QGuiApplication &app, LockWorkerClient::WorkerProcess &worker,
                SaverPresentation &saver, QString user);
  ~NativeWindows() override;
  bool start();
  AuthenticationController *controller() const { return m_controller.get(); }
private:
  bool create(QScreen *screen);
  QGuiApplication &m_app;
  LockWorkerClient::WorkerProcess &m_worker;
  SaverPresentation &m_saver;
  QString m_user;
  QHash<QScreen *, QQuickView *> m_windows;
  std::unique_ptr<AuthenticationController> m_controller;
};
}
