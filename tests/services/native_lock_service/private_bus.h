// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QElapsedTimer>
#include <QProcess>
#include <QtTest>
class PrivateBus final {
public:
  inline static int next=0;
  int id=++next;
  PrivateBus() {
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {"--session", "--nofork", "--print-address=1"});
    if (daemon.waitForStarted() && daemon.waitForReadyRead())
      address = QString::fromUtf8(daemon.readLine()).trimmed();
  }
  ~PrivateBus() {
    for (const auto &name : names)
      QDBusConnection::disconnectFromBus(name);
    daemon.terminate();
    daemon.waitForFinished(5000);
  }
  QDBusConnection connect(const QString &suffix) {
    const auto name = QStringLiteral("lock-%1-%2-%3")
                          .arg(QCoreApplication::applicationPid())
                          .arg(id).arg(suffix);
    names.append(name);
    return QDBusConnection::connectToBus(address, name);
  }
  QProcess daemon;
  QString address;
  QStringList names;
};
inline QDBusMessage waitReply(QDBusPendingCall call) {
  QElapsedTimer timer;
  timer.start();
  while (!call.isFinished() && timer.elapsed() < 5000)
    QTest::qWait(1);
  return call.reply();
}
