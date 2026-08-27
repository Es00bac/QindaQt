// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Audio
{

class AudioServiceObject;

enum class ServiceStartStatus {
    Started,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
};

// Owns the service object/name and backend lifetime on the constructing Qt
// thread. stop() is idempotent, makes operations uncertain, and releases D-Bus
// ownership before destruction. The named Qt connection must remain registered
// until this object is destroyed.
class ResidentAudioService : public QObject
{
    Q_OBJECT

public:
    explicit ResidentAudioService(std::unique_ptr<AudioBackend> backend,
                                  const QDBusConnection &connection,
                                  QString serviceName = {}, QObject *parent = nullptr);
    ~ResidentAudioService() override;

    [[nodiscard]] ServiceStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] AudioOperationCoordinator *coordinator() noexcept;

private:
    std::unique_ptr<AudioBackend> m_backend;
    std::unique_ptr<AudioOperationCoordinator> m_coordinator;
    std::unique_ptr<AudioServiceObject> m_serviceObject;
    QDBusConnection m_connection;
    QString m_serviceName;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
};

} // namespace QindaQt::Audio
