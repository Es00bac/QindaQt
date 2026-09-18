// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_service/resident_audio_service.h>
#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusConnection>

#include <memory>

using namespace QindaQt::Audio;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-audio-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QDBusConnection sessionConnection = QDBusConnection::sessionBus();
    // AGENT-GUARD: This activated process belongs to exactly the bus that
    // constructed it. Bus replacement must terminate the process; reconnecting
    // would expose stale backend/epoch state under a new authority lineage.
    if (!sessionConnection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"),
            QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("Audio1 could not bind constructing-bus lifetime");
        return 1;
    }

    auto backend = std::make_unique<WirePlumberAudioBackend>();
    ResidentAudioService service(std::move(backend), sessionConnection);
    const ServiceStartStatus status = service.start();
    // AGENT-GUARD: NameAlreadyOwned means a live sibling already provides
    // Audio1 -- not a failure of this process. Exiting nonzero here would
    // have systemd's Restart=on-failure retry a start that can only ever
    // fail the same way again while the sibling lives, looping until
    // StartLimitBurst; exit 0 lets the unit settle once, with the reason on
    // the record.
    if (status == ServiceStartStatus::NameAlreadyOwned) {
        qInfo("Audio1 startup stopped: org.qindaqt.Audio1 is already owned "
              "by a live sibling process");
        return 0;
    }
    if (status != ServiceStartStatus::Started) {
        qCritical("Audio1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &ResidentAudioService::stop);
    return QCoreApplication::exec();
}
