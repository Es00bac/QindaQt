// SPDX-License-Identifier: LGPL-3.0-or-later
#include "shortcut_ui.h"
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QTimer>
#include <fcntl.h>
#include <unistd.h>
#include <utility>
namespace QindaQt::Services::Portal {
ProcessShortcuts::ProcessShortcuts(PortalSessionBinding &binding,
                                   AccessConsent &authority, QString executable)
    : m_binding(binding), m_authority(authority),
      m_executable(std::move(executable)) {
  connect(&authority, &AccessConsent::authorityLost, this, [this] {
    cancel(m_token);
    Q_EMIT authorityLost();
  });
  connect(&binding, &PortalSessionBinding::invalidated, this, [this] {
    cancel(m_token);
    Q_EMIT authorityLost();
  });
  connect(&m_process, &QProcess::started, this, [this] {
    if (m_fd >= 0) {
      close(m_fd);
      m_fd = -1;
    }
  });
  connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
    m_output += m_process.readAllStandardOutput();
    if (m_output.size() > 2097152) {
      const auto token = m_token;
      cancel(token);
      fail(token);
    }
  });
  connect(&m_process, &QProcess::readyReadStandardError, this,
          [this] { m_process.readAllStandardError(); });
  connect(&m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
              return;
            const auto token = m_token;
            cancel(token);
            fail(token);
          });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, &ProcessShortcuts::complete);
}
ProcessShortcuts::~ProcessShortcuts() {
  cancel(m_token);
  if (m_process.state() != QProcess::NotRunning) {
    m_process.kill();
    m_process.waitForFinished(1000);
  }
}
bool ProcessShortcuts::admitted() const {
  return m_binding.live() && m_authority.admitted();
}
void ProcessShortcuts::fail(RequestToken token) {
  if (token)
    QTimer::singleShot(0, this, [this, token] {
      Q_EMIT completed(token, RequestResponse::Failed, {});
    });
}
void ProcessShortcuts::start(RequestToken token, const QJsonObject &frame) {
  const auto bytes = QJsonDocument(frame).toJson(QJsonDocument::Compact) + '\n';
  if (!token || !admitted() || m_executable.isEmpty() ||
      m_process.state() != QProcess::NotRunning || bytes.size() > 131072) {
    fail(token);
    return;
  }
  const int fd = m_binding.openDisplay();
  if (fd < 0) {
    fail(token);
    return;
  }
  m_fd = fd;
  m_token = token;
  m_output.clear();
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.remove(QStringLiteral("DISPLAY"));
  environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
  // Public Qt widgets only; no platform-theme portal recursion/KDE chooser.
  environment.remove(QStringLiteral("QT_QPA_PLATFORMTHEME"));
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                     QStringLiteral("wayland"));
  environment.insert(QStringLiteral("QT_STYLE_OVERRIDE"),
                     QStringLiteral("Fusion"));
  environment.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(fd));
  m_process.setProcessEnvironment(environment);
  m_process.setChildProcessModifier([fd] {
    if (fcntl(fd, F_SETFD, 0) < 0)
      _exit(2);
  });
  m_process.start(m_executable, {});
  m_process.write(bytes);
}
void ProcessShortcuts::ask(RequestToken token, const QJsonObject &frame) {
  start(token, frame);
}
void ProcessShortcuts::cancel(RequestToken token) {
  if (!token || token != m_token)
    return;
  m_token = 0;
  m_output.clear();
  if (m_fd >= 0) {
    close(m_fd);
    m_fd = -1;
  }
  if (m_process.state() != QProcess::NotRunning)
    m_process.kill();
}
void ProcessShortcuts::complete(int code, QProcess::ExitStatus status) {
  m_output += m_process.readAllStandardOutput();
  if (m_fd >= 0) {
    close(m_fd);
    m_fd = -1;
  }
  const auto token = std::exchange(m_token, 0);
  if (!token) {
    m_output.clear();
    return;
  }
  RequestResponse response = RequestResponse::Failed;
  QJsonObject results;
  if (code == 0 && status == QProcess::NormalExit &&
      m_output.size() <= 2097152 && admitted()) {
    QJsonParseError error;
    const auto object = QJsonDocument::fromJson(m_output, &error).object();
    if (error.error == QJsonParseError::NoError && object.size() == 2 &&
        object.value("response").isDouble() &&
        object.value("results").isObject()) {
      const int value = object.value("response").toInt(-1);
      if (value == 0 || value == 1) {
        response = static_cast<RequestResponse>(value);
        results = object.value("results").toObject();
      }
    }
  }
  m_output.clear();
  Q_EMIT completed(token, response, results);
}
} // namespace QindaQt::Services::Portal
