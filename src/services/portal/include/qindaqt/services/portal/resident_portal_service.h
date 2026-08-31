// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

#include <memory>

namespace QindaQt::Services::Portal {

class AppearanceSource;

inline constexpr auto kPortalServiceName =
    "org.freedesktop.impl.portal.desktop.qindaqt";
inline constexpr auto kPortalObjectPath = "/org/freedesktop/portal/desktop";
inline constexpr auto kPortalSettingsInterface =
    "org.freedesktop.impl.portal.Settings";
inline constexpr quint32 kPortalSettingsBackendVersion = 1;

enum class PortalServiceStartStatus {
    Started,
    AlreadyRunning,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
    SourceStartFailed,
};

// Owns the standard backend object/name and the lifetime of its injected
// appearance source. The bus connection and source must outlive this object;
// all three are thread-confined to the constructing Qt event-loop thread.
class ResidentPortalService final : public QObject {
    Q_OBJECT
public:
    ResidentPortalService(AppearanceSource &source,
                          QDBusConnection connection,
                          QString serviceName = QString::fromLatin1(kPortalServiceName),
                          QObject *parent = nullptr);
    ~ResidentPortalService() override;

    [[nodiscard]] PortalServiceStartStatus start(QString *error = nullptr);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

private:
    class Private;
    std::unique_ptr<Private> d;
};

[[nodiscard]] QString portalServiceStartStatusName(PortalServiceStartStatus status);

} // namespace QindaQt::Services::Portal
