// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_service/console_store.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
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
    // `consolePath` is where the console persists (ADR-0176); empty selects
    // ConsoleStore::defaultPath(). A test passes a temporary path so it never
    // touches the user's console.
    explicit ResidentAudioService(std::unique_ptr<AudioBackend> backend,
                                  const QDBusConnection &connection,
                                  QString serviceName = {}, QObject *parent = nullptr,
                                  QString consolePath = {});
    ~ResidentAudioService() override;

    [[nodiscard]] ServiceStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] AudioOperationCoordinator *coordinator() noexcept;

private:
    void saveConsole();

    std::unique_ptr<AudioBackend> m_backend;
    std::unique_ptr<AudioOperationCoordinator> m_coordinator;
    ConsoleStore m_consoleStore;
    // Coalesces a burst of fader moves into one write; the store itself
    // skips a write when nothing in the document changed.
    QTimer m_consoleSaveTimer;
    std::unique_ptr<AudioServiceObject> m_serviceObject;
    QDBusConnection m_connection;
    QString m_serviceName;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
};

} // namespace QindaQt::Audio
