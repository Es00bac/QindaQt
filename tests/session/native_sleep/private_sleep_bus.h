// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>
namespace SleepTest {
// Empty activation namespace: never inherit installed user/system services.
class PrivateSleepBus final {
public:
  PrivateSleepBus() {
    QFile config(directory.filePath(QStringLiteral("bus.conf")));
    if (!config.open(QIODevice::WriteOnly)) return;
    config.write("<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen>"
        "<auth>EXTERNAL</auth><policy context=\"default\"><allow send_destination=\"*\"/>"
        "<allow receive_sender=\"*\"/><allow own=\"*\"/></policy></busconfig>");
    config.close();
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
        {QStringLiteral("--config-file=") + config.fileName(), QStringLiteral("--nofork"),
         QStringLiteral("--print-address=1")});
    if (daemon.waitForStarted() && daemon.waitForReadyRead())
      address = QString::fromUtf8(daemon.readLine()).trimmed();
  }
  ~PrivateSleepBus() {
    for (const auto &name : names) QDBusConnection::disconnectFromBus(name);
    daemon.terminate(); daemon.waitForFinished(5000);
  }
  QDBusConnection connect(const QString &suffix) {
    const auto name = QStringLiteral("sleep-%1-%2-%3")
        .arg(QCoreApplication::applicationPid()).arg(++next).arg(suffix);
    names.append(name); return QDBusConnection::connectToBus(address, name);
  }
private:
  inline static int next = 0;
  QTemporaryDir directory;
  QProcess daemon;
  QString address;
  QStringList names;
};
}
