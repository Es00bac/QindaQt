// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtTypes>

namespace QindaQt::ShellTaskList::Producer {

// Asynchronous transport seam for the public org.qindaqt.Compositor1
// authority. Implementations must bind every signal subscription and method
// call to the exact unique owner passed to requestRefresh(); replies carrying
// a different token or owner are fencing input for the client.
//
// AGENT-CONTRACT: One refresh token covers exactly one Windows() read. The
// Compositor1 reference forbids combining its independent panel-visibility
// inventory with Windows(); widening this seam would reintroduce review
// finding P1-1 from candidate 3a5ae17.
class TaskListProducerTransport : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TaskListProducerTransport() override = default;

  [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
  virtual void stop() = 0;
  virtual void requestRefresh(quint64 token, const QString &uniqueOwner) = 0;

Q_SIGNALS:
  // Empty owner means the well-known name is currently unowned.
  void serviceOwnerChanged(const QString &uniqueOwner);
  // WindowsChanged is an invalidation hint that triggers a complete re-read.
  void refreshInvalidated(const QString &uniqueOwner);
  void windowsRead(quint64 token, const QString &uniqueOwner,
                   const QByteArray &payload);
  void refreshFailed(quint64 token, const QString &uniqueOwner,
                     const QString &message);
};

} // namespace QindaQt::ShellTaskList::Producer
