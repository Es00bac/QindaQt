// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_client/settings_transport.h"

#include <QDBusConnection>

#include <memory>

namespace QindaQt::Services::SettingsClient {

class QtSettingsTransport final : public SettingsTransport {
    Q_OBJECT
public:
    explicit QtSettingsTransport(QDBusConnection connection,
                                 QString serviceName = QStringLiteral("org.qindaqt.Settings1"),
                                 QObject *parent = nullptr);
    ~QtSettingsTransport() override;

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override;
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 baseRevision, const QVariantList &operations) override;
    void requestActivation() override;

private Q_SLOTS:
    void handleBusDisconnected();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::SettingsClient
