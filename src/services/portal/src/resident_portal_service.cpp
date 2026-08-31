// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/portal/resident_portal_service.h"

#include "portal_settings_object_p.h"

#include "qindaqt/services/portal/appearance_source.h"

#include <QDBusConnectionInterface>

#include <utility>

namespace QindaQt::Services::Portal {
namespace {

void setError(QString *output, QString value)
{
    if (output != nullptr) {
        *output = std::move(value);
    }
}

} // namespace

class ResidentPortalService::Private final {
public:
    Private(AppearanceSource &appearanceSource,
            QDBusConnection busConnection,
            QString name)
        : source(appearanceSource)
        , connection(std::move(busConnection))
        , serviceName(std::move(name))
        , object(std::make_unique<Portal::Private::PortalSettingsObject>(source))
    {
    }

    AppearanceSource &source;
    QDBusConnection connection;
    QString serviceName;
    std::unique_ptr<Portal::Private::PortalSettingsObject> object;
    bool objectRegistered = false;
    bool nameRegistered = false;
    bool sourceStarted = false;
};

ResidentPortalService::ResidentPortalService(AppearanceSource &source,
                                             QDBusConnection connection,
                                             QString serviceName,
                                             QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>(source, std::move(connection),
                                  std::move(serviceName)))
{
}

ResidentPortalService::~ResidentPortalService()
{
    stop();
}

PortalServiceStartStatus ResidentPortalService::start(QString *error)
{
    if (isRunning()) {
        return PortalServiceStartStatus::AlreadyRunning;
    }
    if (!d->connection.isConnected() || d->connection.interface() == nullptr
        || d->serviceName.isEmpty()) {
        setError(error, QStringLiteral("portal service bus or name is invalid"));
        return PortalServiceStartStatus::InvalidConnection;
    }
    if (!d->connection.registerObject(
            QString::fromLatin1(kPortalObjectPath), d->object.get(),
            QDBusConnection::ExportScriptableSlots
                | QDBusConnection::ExportScriptableSignals
                | QDBusConnection::ExportScriptableProperties)) {
        setError(error, d->connection.lastError().message());
        return PortalServiceStartStatus::ObjectRegistrationFailed;
    }
    d->objectRegistered = true;
    if (!d->connection.registerService(d->serviceName)) {
        const QString owner =
            d->connection.interface()->serviceOwner(d->serviceName).value();
        setError(error, d->connection.lastError().message());
        stop();
        return owner.isEmpty() ? PortalServiceStartStatus::NameRegistrationFailed
                               : PortalServiceStartStatus::NameAlreadyOwned;
    }
    d->nameRegistered = true;
    QString sourceError;
    if (!d->source.start(&sourceError)) {
        setError(error, sourceError);
        stop();
        return PortalServiceStartStatus::SourceStartFailed;
    }
    d->sourceStarted = true;
    return PortalServiceStartStatus::Started;
}

void ResidentPortalService::stop() noexcept
{
    if (d->sourceStarted) {
        d->source.stop();
        d->sourceStarted = false;
    }
    if (d->nameRegistered) {
        d->connection.unregisterService(d->serviceName);
        d->nameRegistered = false;
    }
    if (d->objectRegistered) {
        d->connection.unregisterObject(QString::fromLatin1(kPortalObjectPath));
        d->objectRegistered = false;
    }
}

bool ResidentPortalService::isRunning() const noexcept
{
    return d->objectRegistered && d->nameRegistered && d->sourceStarted;
}

QString portalServiceStartStatusName(PortalServiceStartStatus status)
{
    switch (status) {
    case PortalServiceStartStatus::Started:
        return QStringLiteral("started");
    case PortalServiceStartStatus::AlreadyRunning:
        return QStringLiteral("already-running");
    case PortalServiceStartStatus::InvalidConnection:
        return QStringLiteral("invalid-connection");
    case PortalServiceStartStatus::ObjectRegistrationFailed:
        return QStringLiteral("object-registration-failed");
    case PortalServiceStartStatus::NameAlreadyOwned:
        return QStringLiteral("name-already-owned");
    case PortalServiceStartStatus::NameRegistrationFailed:
        return QStringLiteral("name-registration-failed");
    case PortalServiceStartStatus::SourceStartFailed:
        return QStringLiteral("source-start-failed");
    }
    return QStringLiteral("unknown");
}

} // namespace QindaQt::Services::Portal
