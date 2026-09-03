// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_service/clipboard_host.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Services::Clipboard {

class ClipboardServiceObject;

enum class ServiceStartStatus {
    Started,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
};

// Takes ownership of an adapter whose start() has already been attempted by
// the composition root. start()/stop() govern bus publication; stop() also
// stops the adapter and purges all host content.
class ResidentClipboardService final : public QObject {
    Q_OBJECT
public:
    explicit ResidentClipboardService(
        std::unique_ptr<ClipboardWayland::ClipboardWaylandAdapter> adapter,
        const QDBusConnection &connection, QString serviceName = {},
        quint64 epochSeed = 0, QObject *parent = nullptr);
    ~ResidentClipboardService() override;
    [[nodiscard]] ServiceStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] ClipboardHost *host() noexcept { return m_host.get(); }
    [[nodiscard]] ClipboardWayland::ClipboardWaylandAdapter *adapter() noexcept
    { return m_adapter.get(); }

private:
    Q_SLOT void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                                   const QString &newOwner);
    std::unique_ptr<ClipboardWayland::ClipboardWaylandAdapter> m_adapter;
    std::unique_ptr<ClipboardHost> m_host;
    std::unique_ptr<ClipboardServiceObject> m_object;
    QDBusConnection m_connection;
    QString m_serviceName;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
    bool m_ownerWatchInstalled = false;
};

} // namespace QindaQt::Services::Clipboard
