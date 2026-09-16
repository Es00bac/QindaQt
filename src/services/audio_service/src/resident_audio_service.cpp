// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_service/resident_audio_service.h>

#include "audio_service_object_p.h"

#include <qindaqt/services/audio_protocol/audio_dbus.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtDBus/QDBusConnectionInterface>

namespace QindaQt::Audio
{

ResidentAudioService::ResidentAudioService(std::unique_ptr<AudioBackend> backend,
                                           const QDBusConnection &connection,
                                           QString serviceName, QObject *parent,
                                           QString consolePath)
    : QObject(parent)
    , m_backend(std::move(backend))
    , m_consoleStore(consolePath.isEmpty() ? ConsoleStore::defaultPath()
                                           : std::move(consolePath))
    , m_connection(connection)
    , m_serviceName(serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                         : std::move(serviceName))
{
    Q_ASSERT(m_backend != nullptr);
    registerDBusTypes();
    m_coordinator = std::make_unique<AudioOperationCoordinator>(m_backend.get());
    // Restore BEFORE the backend runs: the first graph publication then binds
    // the user's console rather than a default one that is replaced a moment
    // later, and the virtual endpoints are declared with the user's labels.
    (void)m_consoleStore.load(m_coordinator->consoleModel());
    m_serviceObject =
        std::make_unique<AudioServiceObject>(m_coordinator.get(), m_connection);

    m_consoleSaveTimer.setSingleShot(true);
    m_consoleSaveTimer.setInterval(500);
    connect(&m_consoleSaveTimer, &QTimer::timeout, this,
            &ResidentAudioService::saveConsole);
    // Every publication, not only console operations: a graph change can
    // change nothing the document carries, and the store already skips an
    // identical write, so this costs a serialisation and nothing more.
    connect(m_coordinator.get(), &AudioOperationCoordinator::snapshotChanged, this,
            [this] { m_consoleSaveTimer.start(); });
}

void ResidentAudioService::saveConsole()
{
    (void)m_consoleStore.save(m_coordinator->consoleModel());
}

ResidentAudioService::~ResidentAudioService()
{
    stop();
}

ServiceStartStatus ResidentAudioService::start()
{
    if (isRunning()) {
        return ServiceStartStatus::Started;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return ServiceStartStatus::InvalidConnection;
    }

    const QString objectPath = QString::fromLatin1(kObjectPath);
    if (!m_connection.registerObject(objectPath, m_serviceObject.get(),
                                     QDBusConnection::ExportScriptableSlots
                                         | QDBusConnection::ExportScriptableSignals)) {
        return ServiceStartStatus::ObjectRegistrationFailed;
    }
    m_objectRegistered = true;

    if (!m_connection.registerService(m_serviceName)) {
        const bool alreadyOwned = m_connection.lastError().name()
            == QStringLiteral("org.freedesktop.DBus.Error.NameExists");
        stop();
        return alreadyOwned ? ServiceStartStatus::NameAlreadyOwned
                            : ServiceStartStatus::NameRegistrationFailed;
    }
    m_nameRegistered = true;
    m_coordinator->start();
    return ServiceStartStatus::Started;
}

void ResidentAudioService::stop()
{
    if (m_coordinator != nullptr) {
        // A pending debounce must not be lost to a shutdown.
        m_consoleSaveTimer.stop();
        saveConsole();
        m_coordinator->stop();
    }
    if (m_nameRegistered) {
        m_connection.unregisterService(m_serviceName);
        m_nameRegistered = false;
    }
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
        m_objectRegistered = false;
    }
}

bool ResidentAudioService::isRunning() const noexcept
{
    return m_nameRegistered && m_objectRegistered;
}

AudioOperationCoordinator *ResidentAudioService::coordinator() noexcept
{
    return m_coordinator.get();
}

} // namespace QindaQt::Audio
