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
// AGENT-CONTRACT: One refresh token covers exactly three reads — Windows(),
// Containers(), and ShellVisibilitySnapshot() — delivered as one reply signal
// each. Any read failure fails the whole refresh; the client never joins a
// partial generation.
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
  // Collapses WindowsChanged, ContainerCommitted, and ShellVisibilityChanged:
  // all three are invalidation hints that must trigger a full re-read.
  void refreshInvalidated(const QString &uniqueOwner);
  void windowsRead(quint64 token, const QString &uniqueOwner,
                   const QByteArray &payload);
  void containersRead(quint64 token, const QString &uniqueOwner,
                      const QByteArray &payload);
  void scopeRead(quint64 token, const QString &uniqueOwner,
                 const QByteArray &payload);
  void refreshFailed(quint64 token, const QString &uniqueOwner,
                     const QString &message);
};

} // namespace QindaQt::ShellTaskList::Producer
