// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/process_consent.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QTimer>
#include <fcntl.h>
#include <unistd.h>
#include <utility>
namespace QindaQt::Services::Portal {
ProcessAccessConsent::ProcessAccessConsent(PortalSessionBinding &binding, QDBusConnection bus,
                                           QString executable, QObject *parent)
    : AccessConsent(parent), m_binding(binding), m_executable(std::move(executable)),
      m_transport(bus), m_monitor(m_transport, [&binding](const QString &owner, quint64 pid) {
          return binding.admits(owner, pid);
      }) {
    connect(&binding, &PortalSessionBinding::bindingChanged, &m_monitor,
            &QindaQt::Services::SessionLockState::NativeLockStateMonitor::refresh);
    connect(&binding, &PortalSessionBinding::invalidated, this, [this] {
        m_monitor.refresh(); cancel(m_token); Q_EMIT authorityLost();
    });
    connect(&m_monitor, &QindaQt::Services::SessionLockState::NativeLockStateMonitor::contentMayBeShownChanged,
            this, [this](bool shown) { if (!shown) { cancel(m_token); Q_EMIT authorityLost(); } });
    connect(&m_process, &QProcess::started, this, [this] {
        if (m_fd >= 0) { close(m_fd); m_fd = -1; }
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > 32768) { cancel(m_token); Q_EMIT authorityLost(); }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        // Helper/QML diagnostics may contain application-supplied question text.
        // They are deliberately drained, never forwarded into resident logs.
        m_process.readAllStandardError();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        if (m_fd >= 0) { close(m_fd); m_fd = -1; }
        const auto token = std::exchange(m_token, 0);
        if (token) Q_EMIT completed(token, RequestResponse::Failed, {});
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ProcessAccessConsent::complete);
    m_monitor.start();
}
ProcessAccessConsent::~ProcessAccessConsent() {
    cancel(m_token); m_monitor.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill(); m_process.waitForFinished(1000);
    }
}
bool ProcessAccessConsent::admitted() const { return m_binding.live() && m_monitor.contentMayBeShown(); }
void ProcessAccessConsent::ask(RequestToken token, const AccessQuestion &question) {
    if (!token || !admitted() || m_process.state() != QProcess::NotRunning || m_executable.isEmpty()) {
        QTimer::singleShot(0, this, [this, token] { Q_EMIT completed(token, RequestResponse::Failed, {}); }); return;
    }
    const int fd = m_binding.openDisplay();
    if (fd < 0) { QTimer::singleShot(0, this, [this, token] { Q_EMIT completed(token, RequestResponse::Failed, {}); }); return; }
    m_fd = fd; m_token = token; m_question = question; m_output.clear();
    QJsonArray choices;
    for (const auto &choice : question.choices) {
        QJsonArray options;
        for (const auto &option : choice.options) options.append(QJsonObject{{QStringLiteral("id"), option.id}, {QStringLiteral("label"), option.label}});
        choices.append(QJsonObject{{QStringLiteral("id"), choice.id}, {QStringLiteral("label"), choice.label},
                                   {QStringLiteral("options"), options}, {QStringLiteral("initial"), choice.initial}});
    }
    const QJsonObject frame{{QStringLiteral("app"), question.appId}, {QStringLiteral("parent"), question.parentWindow},
        {QStringLiteral("title"), question.title}, {QStringLiteral("subtitle"), question.subtitle}, {QStringLiteral("body"), question.body},
        {QStringLiteral("deny"), question.denyLabel}, {QStringLiteral("grant"), question.grantLabel},
        {QStringLiteral("icon"), question.icon}, {QStringLiteral("modal"), question.modal}, {QStringLiteral("choices"), choices}};
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("DISPLAY")); environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(fd));
    m_process.setProcessEnvironment(environment);
    m_process.setChildProcessModifier([fd] { if (fcntl(fd, F_SETFD, 0) < 0) _exit(2); });
    m_process.start(m_executable, {});
    m_process.write(QJsonDocument(frame).toJson(QJsonDocument::Compact)); m_process.closeWriteChannel();
}
void ProcessAccessConsent::cancel(RequestToken token) {
    if (!token || token != m_token) return;
    m_token = 0; m_output.clear();
    if (m_fd >= 0) { close(m_fd); m_fd = -1; }
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
}
void ProcessAccessConsent::complete(int code, QProcess::ExitStatus status) {
    m_output += m_process.readAllStandardOutput();
    if (m_fd >= 0) { close(m_fd); m_fd = -1; }
    const auto token = std::exchange(m_token, 0);
    if (!token) { m_output.clear(); return; }
    RequestResponse response = RequestResponse::Failed; ChoiceValues values;
    if (code == 0 && status == QProcess::NormalExit && m_output.size() <= 32768 && admitted()) {
        QJsonParseError error; const auto document = QJsonDocument::fromJson(m_output, &error);
        const auto object = document.object();
        if (error.error == QJsonParseError::NoError && object.size() == 2
            && object.value(QStringLiteral("response")).isDouble()
            && object.value(QStringLiteral("choices")).isArray()) {
            const int value = object.value(QStringLiteral("response")).toInt(-1);
            if (value == 0 || value == 1) response = static_cast<RequestResponse>(value);
            for (const auto &entry : object.value(QStringLiteral("choices")).toArray()) {
                const auto pair = entry.toObject();
                if (pair.size() != 2 || !pair.value(QStringLiteral("id")).isString()
                    || !pair.value(QStringLiteral("value")).isString()) { response = RequestResponse::Failed; break; }
                values.append({pair.value(QStringLiteral("id")).toString(), pair.value(QStringLiteral("value")).toString()});
            }
            if (response == RequestResponse::Success && !validChoiceValues(m_question, values)) response = RequestResponse::Failed;
        }
    }
    m_output.clear(); Q_EMIT completed(token, response, values);
}
} // namespace QindaQt::Services::Portal
