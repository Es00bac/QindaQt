// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_service/resident_settings_service.h"

#include "dbus_service_name_validation_p.h"
#include "settings_object_p.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_encode.h"
#include "qindaqt/settings/layered_settings.h"
#include "qindaqt/settings/settings_document.h"
#include "qindaqt/settings/settings_migration.h"

#include <QDir>
#include <QDBusError>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <utility>

namespace QindaQt::Services::SettingsService {
namespace {

SettingsServiceStartResult failure(SettingsServiceStartStatus status, QString message)
{
    return {.status = status, .message = std::move(message)};
}

bool layerFitsWire(const QVariantMap &values, QString *error)
{
    SettingsProtocol::AggregateValueDecodeBudget aggregate{
        SettingsProtocol::WireContract::MaximumSnapshotValueBytes,
        SettingsProtocol::WireContract::MaximumSnapshotValueNodes};
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        if (!SettingsProtocol::BoundedSettingsValueCodec::validateKey(iterator.key(), error)
            || !SettingsProtocol::encodeBoundedJsonValueForWire(
                    iterator.value(), aggregate, error).has_value()) {
            return false;
        }
    }
    return true;
}

} // namespace

class ResidentSettingsService::Private final {
public:
    Private(QDBusConnection busConnection,
            Settings::SettingsSchema currentSchema,
            Settings::SettingsSchema previousSchema,
            QString defaultsPath,
            QString storagePath)
        : connection(std::move(busConnection))
        , activeSchema(std::move(currentSchema))
        , legacySchema(std::move(previousSchema))
        , profileDefaultsPath(std::move(defaultsPath))
        , userOverridesPath(std::move(storagePath))
    {
    }

    QDBusConnection connection;
    Settings::SettingsSchema activeSchema;
    Settings::SettingsSchema legacySchema;
    QString profileDefaultsPath;
    QString userOverridesPath;
    QString serviceName;
    std::unique_ptr<SettingsRepository> repository;
    std::unique_ptr<QObject> object;
};

ResidentSettingsService::ResidentSettingsService(QDBusConnection connection,
                                                 Settings::SettingsSchema activeSchema,
                                                 Settings::SettingsSchema legacySchema,
                                                 QString profileDefaultsPath,
                                                 QString userOverridesPath)
    : d(std::make_unique<Private>(std::move(connection), std::move(activeSchema),
                                  std::move(legacySchema), std::move(profileDefaultsPath),
                                  std::move(userOverridesPath)))
{
}

ResidentSettingsService::~ResidentSettingsService()
{
    stop();
}

SettingsServiceStartResult ResidentSettingsService::start(const QString &serviceName)
{
    if (isRunning()) {
        return failure(SettingsServiceStartStatus::AlreadyRunning,
                       QStringLiteral("settings service is already running"));
    }
    QString nameError;
    if (!::QindaQt::Services::SettingsService::Private::validateWellKnownServiceName(
            serviceName, &nameError)) {
        return failure(SettingsServiceStartStatus::NameOwnershipConflict, nameError);
    }
    if (!d->connection.isConnected()) {
        return failure(SettingsServiceStartStatus::BusUnavailable,
                       QStringLiteral("session bus is not connected"));
    }
    const QFileInfo storage(d->userOverridesPath);
    if (!storage.isAbsolute() || storage.fileName().isEmpty()) {
        return failure(SettingsServiceStartStatus::InvalidStoragePath,
                       QStringLiteral("settings storage path must name an absolute file"));
    }

    const QFileInfo profileDefaults(d->profileDefaultsPath);
    if (!profileDefaults.isAbsolute() || profileDefaults.fileName().isEmpty()) {
        return failure(SettingsServiceStartStatus::InvalidProfileDefaultsPath,
                       QStringLiteral("profile-defaults path must name an absolute file"));
    }

    Settings::LayeredSettings initial(d->activeSchema);
    QString wireError;
    if (!layerFitsWire(d->activeSchema.systemDefaults(), &wireError)) {
        return failure(SettingsServiceStartStatus::ServerRegistrationFailed,
                       QStringLiteral("schema defaults are not Settings1-compatible: ")
                           + wireError);
    }
    const auto loadedProfile = Settings::SettingsCompatibilityLoader::load(
        d->profileDefaultsPath, d->activeSchema, d->legacySchema);
    if (!loadedProfile.ok
        || loadedProfile.document.layer != Settings::SettingLayer::ProfileDefaults) {
        return failure(SettingsServiceStartStatus::CorruptProfileDefaults,
                       loadedProfile.ok
                           ? QStringLiteral("settings document is not profile defaults")
                           : loadedProfile.error);
    }
    if (!layerFitsWire(loadedProfile.document.values, &wireError)) {
        return failure(SettingsServiceStartStatus::CorruptProfileDefaults,
                       QStringLiteral("profile defaults exceed Settings1 bounds: ")
                           + wireError);
    }
    const auto appliedProfile = initial.replaceLayer(
        Settings::SettingLayer::ProfileDefaults, loadedProfile.document.values);
    if (!appliedProfile.ok()) {
        return failure(SettingsServiceStartStatus::CorruptProfileDefaults,
                       appliedProfile.message);
    }

    bool migrationPending = false;
    if (storage.exists()) {
        const auto loaded = Settings::SettingsCompatibilityLoader::load(
            d->userOverridesPath, d->activeSchema, d->legacySchema);
        if (!loaded.ok || loaded.document.layer != Settings::SettingLayer::UserOverrides) {
            return failure(SettingsServiceStartStatus::CorruptUserOverrides,
                           loaded.ok ? QStringLiteral("settings document is not user overrides")
                                     : loaded.error);
        }
        if (!layerFitsWire(loaded.document.values, &wireError)) {
            return failure(SettingsServiceStartStatus::CorruptUserOverrides,
                           QStringLiteral("user overrides exceed Settings1 bounds: ")
                               + wireError);
        }
        const auto applied = initial.replaceLayer(loaded.document.layer, loaded.document.values);
        if (!applied.ok()) {
            return failure(SettingsServiceStartStatus::CorruptUserOverrides, applied.message);
        }
        migrationPending = loaded.sourceSchemaVersion == d->legacySchema.version();
    }

    if (!d->connection.registerService(serviceName)) {
        return failure(SettingsServiceStartStatus::NameOwnershipConflict,
                       d->connection.lastError().message());
    }
    d->serviceName = serviceName;

    QDir parent = storage.dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        stop();
        return failure(SettingsServiceStartStatus::InvalidStoragePath,
                       QStringLiteral("cannot create settings storage directory"));
    }
    QFile::setPermissions(parent.absolutePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    if (migrationPending) {
        const Settings::SettingsDocument migrated{
            .schemaVersion = d->activeSchema.version(),
            .layer = Settings::SettingLayer::UserOverrides,
            .values = initial.layerValues(Settings::SettingLayer::UserOverrides)};
        QString saveError;
        if (!Settings::SettingsFileStore::save(d->userOverridesPath, migrated,
                                               d->activeSchema, nullptr, &saveError)) {
            stop();
            return failure(SettingsServiceStartStatus::MigrationPersistFailed, saveError);
        }
    }

    d->repository = std::make_unique<SettingsRepository>(
        std::move(initial), d->userOverridesPath,
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    auto object = std::make_unique<
        ::QindaQt::Services::SettingsService::Private::SettingsObject>(
        d->connection, *d->repository);
    if (!d->connection.registerObject(
            QString::fromLatin1(SettingsProtocol::WireContract::ObjectPath), object.get(),
            QDBusConnection::ExportAllSlots)) {
        stop();
        return failure(SettingsServiceStartStatus::ObjectRegistrationFailed,
                       d->connection.lastError().message());
    }
    d->object = std::move(object);
    return {.status = SettingsServiceStartStatus::Started, .message = {}};
}

void ResidentSettingsService::stop() noexcept
{
    d->object.reset();
    d->connection.unregisterObject(QString::fromLatin1(SettingsProtocol::WireContract::ObjectPath));
    if (!d->serviceName.isEmpty()) {
        d->connection.unregisterService(d->serviceName);
    }
    d->repository.reset();
    d->serviceName.clear();
}

bool ResidentSettingsService::isRunning() const noexcept
{
    return d->repository != nullptr && !d->serviceName.isEmpty();
}

quint64 ResidentSettingsService::revision() const noexcept
{
    return d->repository ? d->repository->revision() : 0;
}

const QString &ResidentSettingsService::epoch() const noexcept
{
    static const QString empty;
    return d->repository ? d->repository->epoch() : empty;
}

QString settingsServiceStartStatusName(SettingsServiceStartStatus status)
{
    switch (status) {
    case SettingsServiceStartStatus::Started: return QStringLiteral("started");
    case SettingsServiceStartStatus::AlreadyRunning: return QStringLiteral("already-running");
    case SettingsServiceStartStatus::InvalidStoragePath: return QStringLiteral("invalid-storage-path");
    case SettingsServiceStartStatus::InvalidProfileDefaultsPath: return QStringLiteral("invalid-profile-defaults-path");
    case SettingsServiceStartStatus::CorruptProfileDefaults: return QStringLiteral("corrupt-profile-defaults");
    case SettingsServiceStartStatus::CorruptUserOverrides: return QStringLiteral("corrupt-user-overrides");
    case SettingsServiceStartStatus::BusUnavailable: return QStringLiteral("bus-unavailable");
    case SettingsServiceStartStatus::BusQueryFailed: return QStringLiteral("bus-query-failed");
    case SettingsServiceStartStatus::NameOwnershipConflict: return QStringLiteral("name-ownership-conflict");
    case SettingsServiceStartStatus::ObjectRegistrationFailed: return QStringLiteral("object-registration-failed");
    case SettingsServiceStartStatus::ServerRegistrationFailed: return QStringLiteral("server-registration-failed");
    case SettingsServiceStartStatus::MigrationPersistFailed: return QStringLiteral("migration-persist-failed");
    }
    return QStringLiteral("unknown");
}

} // namespace QindaQt::Services::SettingsService
