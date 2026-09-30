// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QProcess>
#include <QStringList>
#include <QUuid>
namespace QindaQt::TestSupport {
class PrivateSessionBus final {
public:
  ~PrivateSessionBus() {
    stop();
    for (const auto &name : m_connectionNames)
      QDBusConnection::disconnectFromBus(name);
  }

  bool start(QString *error = nullptr) {
    m_process.start(QStringLiteral("dbus-daemon"),
                    {QStringLiteral("--session"), QStringLiteral("--nofork"),
                     QStringLiteral("--nopidfile"),
                     QStringLiteral("--print-address=1")});
    if (!m_process.waitForStarted(5'000) ||
        !m_process.waitForReadyRead(5'000)) {
      if (error)
        *error = m_process.errorString();
      return false;
    }
    m_address = QString::fromUtf8(m_process.readLine()).trimmed();
    if (m_address.isEmpty()) {
      if (error)
        *error = QStringLiteral("private dbus-daemon published no address");
      return false;
    }
    return true;
  }

  QDBusConnection connect(const QString &label) {
    const auto name =
        label + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_connectionNames.append(name);
    return QDBusConnection::connectToBus(m_address, name);
  }

  void stop() noexcept {
    if (m_process.state() == QProcess::NotRunning) {
      return;
    }
    m_process.terminate();
    if (!m_process.waitForFinished(1'000)) {
      m_process.kill();
      m_process.waitForFinished(1'000);
    }
  }

  const QString &address() const noexcept { return m_address; }

private:
  QStringList m_connectionNames;
  QProcess m_process;
  QString m_address;
};
} // namespace QindaQt::TestSupport
