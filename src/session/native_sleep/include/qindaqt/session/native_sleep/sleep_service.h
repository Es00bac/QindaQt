// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <qindaqt/session/native_sleep/sleep_coordinator.h>
#include <optional>
class QDBusServiceWatcher;
namespace QindaQt::Session::NativeSleep {
// Supervisor-owned public manual handoff. Claims only Sleep1, requires the
// connection's own Session1 owner, and admits actual daemon-resolved caller UID.
// Same-thread borrowed coordinator outlives the facade. No queue or replay.
class SleepService final : public QDBusVirtualObject {
  Q_OBJECT
public:
  SleepService(QDBusConnection bus, SleepCoordinator &coordinator,
               quint32 sessionUid, QObject *parent = nullptr);
  ~SleepService() override;
  bool start();
  void stop();
  QString introspect(const QString &path) const override;
  bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
private:
  void finish(SleepResult result);
  QDBusConnection m_bus;
  SleepCoordinator &m_coordinator;
  const quint32 m_uid;
  QDBusServiceWatcher *m_supervisorWatcher = nullptr;
  std::optional<QDBusMessage> m_pending;
  quint64 m_generation = 0;
  bool m_started = false;
};
}
