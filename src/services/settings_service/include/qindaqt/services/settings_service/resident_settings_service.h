// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnection>
#include <QString>

#include <memory>

namespace QindaQt::Services::SettingsService {

enum class SettingsServiceStartStatus {
    Started,
    AlreadyRunning,
    InvalidStoragePath,
    InvalidProfileDefaultsPath,
    CorruptProfileDefaults,
    CorruptUserOverrides,
    BusUnavailable,
    BusQueryFailed,
    NameOwnershipConflict,
    ObjectRegistrationFailed,
    ServerRegistrationFailed,
    MigrationPersistFailed,
};

struct SettingsServiceStartResult final {
    SettingsServiceStartStatus status = SettingsServiceStartStatus::ServerRegistrationFailed;
    QString message;

    [[nodiscard]] bool ok() const noexcept { return status == SettingsServiceStartStatus::Started; }
};

[[nodiscard]] QString settingsServiceStartStatusName(SettingsServiceStartStatus status);

// Resident composition root for org.qindaqt.Settings1: owns the D-Bus
// registration lifecycle, the schema pair (active v2 plus legacy v1 for
// migration), one mandatory profile-defaults document, one optional user file,
// and the SettingsRepository/private D-Bus object pair. Paths and schemas are
// copied into the service; the connection remains Qt-owned. Calls and emitted
// D-Bus traffic stay on the constructing thread.
// AGENT-CONTRACT: a document migrated from v1 is persisted to disk only
// *after* this process has won the well-known service name. A losing
// concurrent instance during a startup race therefore can never overwrite
// the winner's freshly migrated file -- see
// docs/wiki/adr/0012-persist-notification-quieting-through-settings1.md. Corrupt or
// unsupported-version profile/user data fails start() outright rather than
// silently discarding it. An installed v1 profile is migrated only in memory;
// a v1 user file is replaced only after name ownership succeeds.
class ResidentSettingsService final {
public:
    ResidentSettingsService(QDBusConnection connection, Settings::SettingsSchema activeSchema,
                            Settings::SettingsSchema legacySchema,
                            QString profileDefaultsPath,
                            QString userOverridesPath);
    ~ResidentSettingsService();

    ResidentSettingsService(const ResidentSettingsService &) = delete;
    ResidentSettingsService &operator=(const ResidentSettingsService &) = delete;
    ResidentSettingsService(ResidentSettingsService &&) = delete;
    ResidentSettingsService &operator=(ResidentSettingsService &&) = delete;

    [[nodiscard]] SettingsServiceStartResult start(
        const QString &serviceName = QStringLiteral("org.qindaqt.Settings1"));
    void stop() noexcept;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] quint64 revision() const noexcept;
    [[nodiscard]] const QString &epoch() const noexcept;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::SettingsService
