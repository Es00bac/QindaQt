// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/power_transport.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>
#include <qindaqt/services/power_service/power_collaborators.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>
#include <qindaqt/services/power_service/resident_power_service.h>
#include <qindaqt/services/power_service/unavailable_power_collaborators.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>

#include <iostream>

using namespace QindaQt::Power;

namespace {

class NullTransport final : public PowerTransport
{
public:
    void start() override {}
    void stop() override {}
    void fetchSnapshot(const QString &owner, quint64 requestId) override
    {
        Q_EMIT snapshotReply(owner, requestId, false, {},
                             QStringLiteral("owner-unavailable"));
    }
    void submitOperation(const QString &owner, quint64 requestId,
                         const PowerClientRequest &) override
    {
        Q_EMIT operationReply(owner, requestId, false, {},
                              QStringLiteral("owner-unavailable"));
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    Snapshot snapshot;
    snapshot.epoch = 7;
    snapshot.revision = 1;
    snapshot.availability = Availability::Ready;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.capabilities = Capability::Supplies;
    if (!validateSnapshot(snapshot).accepted) {
        std::cerr << "installed protocol validation rejected a valid snapshot\n";
        return 1;
    }

    NullTransport transport;
    PowerClient client(&transport);
    client.start();
    if (client.state() != PowerClientState::Starting || client.hasSnapshot()) {
        std::cerr << "installed client failed to enter discovery state\n";
        return 1;
    }
    client.stop();
    if (client.state() != PowerClientState::Stopped) {
        std::cerr << "installed client failed to stop\n";
        return 1;
    }

    UnavailableBatteryCollaborator battery;
    UnavailableProfileCollaborator profiles;
    UnavailableSessionCollaborator session;
    PowerServiceCoordinator coordinator(&battery, &profiles, &session);
    coordinator.start();
    QTimer::singleShot(0, &application, &QCoreApplication::quit);
    QCoreApplication::exec();
    const Snapshot unavailable = coordinator.snapshot();
    if (unavailable.availability != Availability::Unavailable
        || unavailable.reasonCode != QStringLiteral("upstream-not-integrated")
        || unavailable.capabilities != Capabilities{}) {
        std::cerr << "installed coordinator published unexpected unavailable truth\n";
        return 1;
    }
    coordinator.stop();

    ResidentPowerService resident(std::make_unique<UnavailableBatteryCollaborator>(),
                                  std::make_unique<UnavailableProfileCollaborator>(),
                                  std::make_unique<UnavailableSessionCollaborator>(),
                                  QDBusConnection(QStringLiteral("invalid")));
    if (resident.start() != PowerServiceStartStatus::InvalidConnection) {
        std::cerr << "installed resident accepted an invalid connection\n";
        return 1;
    }

    std::cout << "installed Power1 consumer passed\n";
    return 0;
}
