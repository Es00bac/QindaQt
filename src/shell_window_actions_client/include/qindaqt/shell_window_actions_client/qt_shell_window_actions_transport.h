// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_window_actions_client/shell_window_actions_transport.h"

#include <QDBusConnection>
#include <QList>
#include <QPointer>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::ShellWindowActionsClient {

class QtShellWindowActionsTransport final : public ShellWindowActionsTransport
{
    Q_OBJECT

public:
    explicit QtShellWindowActionsTransport(QDBusConnection connection,
                                           QObject *parent = nullptr);
    ~QtShellWindowActionsTransport() override;
    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    void request(quint64 token, const QString &uniqueOwner,
                 Compositor::ShellWindowAction action,
                 const QString &windowId,
                 const Compositor::ShellWindowGeneration &generation) override;
    void requestIdentity(quint64 token, const QString &uniqueOwner) override;

private Q_SLOTS:
    void handleIdentityInvalidation();

private:
    void resolveOwner();
    void bindOwner(const QString &uniqueOwner);
    void fail(quint64 token, const QString &uniqueOwner, QString message);
    void failIdentity(quint64 token, const QString &uniqueOwner, QString message);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QList<QPointer<QDBusPendingCallWatcher>> m_pendingCalls;
    QString m_uniqueOwner;
    quint64 m_ownerGeneration = 0;
    bool m_identitySignalConnected = false;
    bool m_started = false;
};

} // namespace QindaQt::ShellWindowActionsClient
