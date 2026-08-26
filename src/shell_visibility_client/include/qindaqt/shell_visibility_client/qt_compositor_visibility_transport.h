// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility_client/compositor_visibility_transport.h"

#include <QDBusConnection>
#include <QList>
#include <QString>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::ShellVisibilityClient {

class QtCompositorVisibilityTransport final : public CompositorVisibilityTransport {
    Q_OBJECT

public:
    explicit QtCompositorVisibilityTransport(
        QDBusConnection connection = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);
    ~QtCompositorVisibilityTransport() override;

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    void requestSnapshot(quint64 token, const QString &uniqueOwner) override;

private Q_SLOTS:
    void handleInvalidation();

private:
    void resolveInitialOwner();
    void bindOwner(const QString &uniqueOwner);
    void failRequest(quint64 token, const QString &uniqueOwner, QString message);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QList<QDBusPendingCallWatcher *> m_pendingCalls;
    QString m_uniqueOwner;
    quint64 m_resolutionGeneration = 0;
    bool m_started = false;
};

} // namespace QindaQt::ShellVisibilityClient
