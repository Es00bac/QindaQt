// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemd_user_units.h"

#include <QProcess>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

QString SystemctlUserUnits::run(const QStringList &arguments) {
  QProcess process;
  process.setProgram(QStringLiteral("systemctl"));
  // AGENT-GUARD: "--user" is not optional. Without it these calls would ask
  // the system manager, which would prompt for authentication and act outside
  // the user's own session.
  process.setArguments(QStringList{QStringLiteral("--user")} + arguments);
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start();
  if (!process.waitForStarted(timeoutMilliseconds)) {
    return QStringLiteral("systemctl could not be started");
  }
  if (!process.waitForFinished(timeoutMilliseconds)) {
    process.kill();
    process.waitForFinished(timeoutMilliseconds);
    return QStringLiteral("systemctl did not answer in time");
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    const QString output = QString::fromUtf8(process.readAll()).trimmed();
    return output.isEmpty()
               ? QStringLiteral("systemctl refused the request")
               : output.left(512);
  }
  return {};
}

QString SystemctlUserUnits::reload() {
  return run({QStringLiteral("daemon-reload")});
}

QString SystemctlUserUnits::enable(const QString &unitName) {
  return run({QStringLiteral("enable"), unitName});
}

QString SystemctlUserUnits::disable(const QString &unitName) {
  return run({QStringLiteral("disable"), unitName});
}

} // namespace QindaQt::Apps::FileManager
