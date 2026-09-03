// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include "qindaqt/shell_window_actions_client/shell_window_actions_transport.h"

#include <limits>
#include <utility>

namespace QindaQt::ShellWindowActionsClient {
namespace {

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

} // namespace

ShellWindowActionsClient::ShellWindowActionsClient(
    ShellWindowActionsTransport &transport,
    int timeoutMilliseconds,
    QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_timeoutMilliseconds(timeoutMilliseconds)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finishUncertain(QStringLiteral("request-timeout"),
                        QStringLiteral("the shell window action outcome is uncertain"));
    });
    connect(&m_transport, &ShellWindowActionsTransport::serviceOwnerChanged,
            this, &ShellWindowActionsClient::handleOwner);
    connect(&m_transport, &ShellWindowActionsTransport::replyReceived,
            this, &ShellWindowActionsClient::handleReply);
    connect(&m_transport, &ShellWindowActionsTransport::requestFailed,
            this, &ShellWindowActionsClient::handleFailure);
}

ShellWindowActionsClient::~ShellWindowActionsClient()
{
    stop();
}

bool ShellWindowActionsClient::start(QString *error)
{
    if (m_started) {
        if (error) error->clear();
        return true;
    }
    if (m_timeoutMilliseconds <= 0 || m_timeoutMilliseconds > 60'000) {
        setError(error, QStringLiteral("shell action timeout is invalid"));
        return false;
    }
    m_started = true;
    if (!m_transport.start(error)) {
        m_started = false;
        return false;
    }
    if (error) error->clear();
    return true;
}

void ShellWindowActionsClient::stop()
{
    if (!m_started) return;
    const bool wasAvailable = available();
    m_started = false;
    m_uniqueOwner.clear();
    if (m_pending) {
        finishUncertain(QStringLiteral("client-stopped"),
                        QStringLiteral("the client stopped before the action reply"));
    }
    m_timeout.stop();
    m_transport.stop();
    if (wasAvailable) Q_EMIT availabilityChanged();
}

bool ShellWindowActionsClient::request(
    Compositor::ShellWindowAction action,
    const QString &windowId,
    const Compositor::ShellWindowGeneration &generation,
    QString *error)
{
    if (!m_started || m_uniqueOwner.isEmpty()) {
        setError(error, QStringLiteral("the compositor action service is unavailable"));
        return false;
    }
    if (m_pending) {
        setError(error, QStringLiteral("a shell window action is already in flight"));
        return false;
    }
    if (!generation.isValid() || windowId.isEmpty()) {
        setError(error, QStringLiteral("the action target or generation is invalid"));
        return false;
    }
    if (m_nextToken == 0) {
        setError(error, QStringLiteral("the action request token is exhausted"));
        return false;
    }
    const quint64 token = m_nextToken;
    m_nextToken = token == std::numeric_limits<quint64>::max() ? 0 : token + 1;
    m_pending = Pending{token, action, windowId, generation, m_uniqueOwner};
    m_timeout.start(m_timeoutMilliseconds);
    m_transport.request(token, m_uniqueOwner, action, windowId, generation);
    if (error) error->clear();
    return true;
}

bool ShellWindowActionsClient::available() const noexcept
{
    return m_started && !m_uniqueOwner.isEmpty();
}

bool ShellWindowActionsClient::requestInFlight() const noexcept
{
    return m_pending.has_value();
}

const QString &ShellWindowActionsClient::uniqueOwner() const noexcept
{
    return m_uniqueOwner;
}

const std::optional<ShellWindowActionClientResult> &
ShellWindowActionsClient::lastResult() const noexcept
{
    return m_lastResult;
}

void ShellWindowActionsClient::handleOwner(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner == m_uniqueOwner) return;
    const bool wasAvailable = available();
    // AGENT-GUARD: Publish the new binding before actionFinished. A slot may
    // synchronously submit its next intent and must never target the old owner.
    m_uniqueOwner = uniqueOwner;
    if (m_pending) {
        finishUncertain(QStringLiteral("owner-changed"),
                        QStringLiteral("the compositor owner changed during the action"));
    }
    if (wasAvailable != available()) Q_EMIT availabilityChanged();
}

void ShellWindowActionsClient::handleReply(
    quint64 token, const QString &uniqueOwner, const QByteArray &payload)
{
    if (!m_started || !m_pending || m_pending->token != token
        || m_pending->uniqueOwner != uniqueOwner || m_uniqueOwner != uniqueOwner) {
        return;
    }
    const Pending pending = *m_pending;
    QString error;
    const auto result = Compositor::decodeShellWindowActionResult(payload, &error);
    if (!result || result->action != pending.action
        || result->windowId != pending.windowId
        || result->generation != pending.generation) {
        finishUncertain(QStringLiteral("malformed-reply"),
                        error.isEmpty()
                            ? QStringLiteral("the action reply did not match its request")
                            : error);
        return;
    }
    m_timeout.stop();
    m_pending.reset();
    m_lastResult = ShellWindowActionClientResult{
        pending.token, pending.action, pending.windowId, pending.generation,
        *result, {}, {}, false};
    Q_EMIT actionFinished();
}

void ShellWindowActionsClient::handleFailure(
    quint64 token, const QString &uniqueOwner, const QString &message)
{
    if (!m_started || !m_pending || m_pending->token != token
        || m_pending->uniqueOwner != uniqueOwner) {
        return;
    }
    finishUncertain(QStringLiteral("transport-failed"),
                    message.isEmpty() ? QStringLiteral("the D-Bus action failed") : message);
}

void ShellWindowActionsClient::finishUncertain(QString code, QString message)
{
    if (!m_pending) return;
    const Pending pending = *m_pending;
    m_timeout.stop();
    m_pending.reset();
    m_lastResult = ShellWindowActionClientResult{
        pending.token, pending.action, pending.windowId, pending.generation,
        std::nullopt, std::move(code), std::move(message), true};
    Q_EMIT actionFinished();
}

} // namespace QindaQt::ShellWindowActionsClient
