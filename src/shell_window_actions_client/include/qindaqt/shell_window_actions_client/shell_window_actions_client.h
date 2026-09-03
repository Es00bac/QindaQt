// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowactions.h"

#include <QObject>
#include <QTimer>

#include <optional>

namespace QindaQt::ShellWindowActionsClient {

class ShellWindowActionsTransport;

struct ShellWindowActionClientResult final
{
    quint64 token = 0;
    Compositor::ShellWindowAction action = Compositor::ShellWindowAction::Activate;
    QString windowId;
    Compositor::ShellWindowGeneration generation;
    std::optional<Compositor::ShellWindowActionResult> serverResult;
    QString failureCode;
    QString message;
    bool uncertain = false;
};

// One-in-flight exact-owner client. Timeout, transport loss, or owner change
// after dispatch is uncertain and is never retried automatically.
class ShellWindowActionsClient final : public QObject
{
    Q_OBJECT

public:
    explicit ShellWindowActionsClient(ShellWindowActionsTransport &transport,
                                      int timeoutMilliseconds = 2000,
                                      QObject *parent = nullptr);
    ~ShellWindowActionsClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    [[nodiscard]] bool request(Compositor::ShellWindowAction action,
                               const QString &windowId,
                               const Compositor::ShellWindowGeneration &generation,
                               QString *error = nullptr);
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool requestInFlight() const noexcept;
    [[nodiscard]] const QString &uniqueOwner() const noexcept;
    [[nodiscard]] const std::optional<ShellWindowActionClientResult> &
    lastResult() const noexcept;

Q_SIGNALS:
    void availabilityChanged();
    void actionFinished();

private:
    struct Pending final
    {
        quint64 token = 0;
        Compositor::ShellWindowAction action = Compositor::ShellWindowAction::Activate;
        QString windowId;
        Compositor::ShellWindowGeneration generation;
        QString uniqueOwner;
    };

    void handleOwner(const QString &uniqueOwner);
    void handleReply(quint64 token, const QString &uniqueOwner,
                     const QByteArray &payload);
    void handleFailure(quint64 token, const QString &uniqueOwner,
                       const QString &message);
    void finishUncertain(QString code, QString message);

    ShellWindowActionsTransport &m_transport;
    QTimer m_timeout;
    std::optional<Pending> m_pending;
    std::optional<ShellWindowActionClientResult> m_lastResult;
    QString m_uniqueOwner;
    quint64 m_nextToken = 1;
    int m_timeoutMilliseconds = 2000;
    bool m_started = false;
};

} // namespace QindaQt::ShellWindowActionsClient
