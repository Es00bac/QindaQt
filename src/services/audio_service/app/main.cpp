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
    if (status != ServiceStartStatus::Started) {
        qCritical("Audio1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &ResidentAudioService::stop);
    return QCoreApplication::exec();
}
