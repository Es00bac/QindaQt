// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_client/power_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Power {

// AGENT-CONTRACT: QtDBus implementation of the Power1 transport seam on one
// explicit bus connection. All requests and the Changed subscription are
// addressed to the exact current unique owner, never the well-known name. The
// borrowed connection must outlive this object on its thread; late replies are
// delivered with the owner they were addressed to so the client can fence
// them.
class QtPowerTransport : public PowerTransport
{
    Q_OBJECT

public:
    explicit QtPowerTransport(const QDBusConnection &connection,
                              QString serviceName = {}, QObject *parent = nullptr);
    ~QtPowerTransport() override;

    void start() override;
    void stop() override;
    void fetchSnapshot(const QString &owner, quint64 requestId) override;
    void submitOperation(const QString &owner, quint64 requestId,
                         const PowerClientRequest &request) override;

private:
    void queryInitialOwner();
    void setOwner(const QString &owner);

private Q_SLOTS:
    void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                               const QString &newOwner);
    void onChanged(quint64 epoch, quint64 revision);

private:
    class Private;

    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Power
