// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QString>
namespace QindaQt::Services::SessionLockState {
// Read-only asynchronous platform port. No request-lock, greeter, approval or
// unlock capability. Borrowed by the monitor; same affinity thread/lifetime.
class NativeLockTransport : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  virtual bool start(QString *error = nullptr) = 0;
  virtual void stop() = 0;
  virtual void requestOwner(quint64 generation) = 0;
  virtual void requestPid(quint64 generation, const QString &uniqueOwner) = 0;
  virtual bool subscribe(const QString &uniqueOwner) = 0;
  virtual void unsubscribe() = 0;
  virtual void requestState(quint64 generation, quint64 serial,
                            const QString &uniqueOwner) = 0;
Q_SIGNALS:
  void lost();
  void ownerChanged();
  void ownerResolved(quint64 generation, const QString &uniqueOwner);
  void pidResolved(quint64 generation, const QString &uniqueOwner,
                   quint64 daemonPid);
  void stateResolved(quint64 generation, quint64 serial,
                     const QString &uniqueOwner, bool locked,
                     bool protectedPresentation);
  void stateInvalidated(const QString &uniqueOwner);
  void failed(quint64 generation, quint64 serial, const QString &uniqueOwner,
              const QString &error);
};
} // namespace QindaQt::Services::SessionLockState
