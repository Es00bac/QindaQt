// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowactions.h"

#include <QByteArray>
#include <QObject>
#include <QString>

namespace QindaQt::ShellWindowActionsClient {

// Implementations resolve the well-known compositor name but send each action
// only to the exact unique owner supplied by the client.
class ShellWindowActionsTransport : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~ShellWindowActionsTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    virtual void request(quint64 token,
                         const QString &uniqueOwner,
                         Compositor::ShellWindowAction action,
                         const QString &windowId,
                         const Compositor::ShellWindowGeneration &generation) = 0;

Q_SIGNALS:
    void serviceOwnerChanged(const QString &uniqueOwner);
    void replyReceived(quint64 token, const QString &uniqueOwner,
                       const QByteArray &payload);
    void requestFailed(quint64 token, const QString &uniqueOwner,
                       const QString &message);
};

} // namespace QindaQt::ShellWindowActionsClient
