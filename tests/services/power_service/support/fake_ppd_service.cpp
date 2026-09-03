// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_ppd_service.h"


#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusObjectPath>

#include <QtCore/QVariant>

namespace QindaQt::Tests {
namespace {

constexpr char kObjectPath[] = "/net/hadess/PowerProfiles";
constexpr char kPrimaryName[] = "org.freedesktop.UPower.PowerProfiles";
constexpr char kLegacyName[] = "net.hadess.PowerProfiles";
constexpr char kPrimaryInterface[] = "org.freedesktop.UPower.PowerProfiles";
constexpr char kLegacyInterface[] = "net.hadess.PowerProfiles";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

QVariant arrayOfStringVariantMaps(const QList<QVariantMap> &entries)
{
    QDBusArgument argument;
    argument.beginArray(QMetaType::fromType<QVariantMap>());
    for (const QVariantMap &entry : entries) {
        argument << entry;
    }
    argument.endArray();
    return QVariant::fromValue(argument);
}

} // namespace

FakePpdService::FakePpdService(const QDBusConnection &connection, const bool legacyOnly,
                               QObject *parent)
    : QDBusVirtualObject(parent)
    , m_connection(connection)
    , m_legacyOnly(legacyOnly)
{
}

FakePpdService::~FakePpdService()
{
    unregisterService();
}

bool FakePpdService::registerService()
{
    if (!m_connection.registerService(busName())) {
        return false;
    }
    return m_connection.registerVirtualObject(QString::fromLatin1(kObjectPath), this,
                                              QDBusConnection::SubPath);
}

void FakePpdService::unregisterService()
{
    m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
    m_connection.unregisterService(busName());
}

QString FakePpdService::busName() const
{
    return QString::fromLatin1(m_legacyOnly ? kLegacyName : kPrimaryName);
}

QString FakePpdService::interfaceName() const
{
    return QString::fromLatin1(m_legacyOnly ? kLegacyInterface : kPrimaryInterface);
}

QString FakePpdService::holdsKey() const
{
    return m_legacyOnly ? QStringLiteral("Holds") : QStringLiteral("ActiveProfileHolds");
}

void FakePpdService::setProfiles(const QStringList &profileIds)
{
    m_profiles = profileIds;
}

void FakePpdService::setActiveProfile(const QString &profileId)
{
    m_activeProfile = profileId;
}

void FakePpdService::setHolds(const QList<HoldSpec> &holds)
{
    m_holds = holds;
}

void FakePpdService::setRejectSetProfile(const bool reject)
{
    m_rejectSetProfile = reject;
}

void FakePpdService::emitPropertiesChanged()
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kObjectPath), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"));
    QVariantMap changed;
    changed.insert(QStringLiteral("ActiveProfile"), QVariant(m_activeProfile));
    changed.insert(QStringLiteral("Profiles"), profilesValue());
    changed.insert(holdsKey(), holdsValue());
    signal.setArguments({interfaceName(), QVariant(changed), QStringList{}});
    m_connection.send(signal);
}

QVariant FakePpdService::profilesValue() const
{
    QList<QVariantMap> entries;
    for (const QString &id : m_profiles) {
        QVariantMap entry;
        entry.insert(QStringLiteral("Profile"), QVariant(id));
        entry.insert(QStringLiteral("Driver"),
                     QVariant(QStringLiteral("fake-driver")));
        entries.push_back(entry);
    }
    return arrayOfStringVariantMaps(entries);
}

QVariant FakePpdService::holdsValue() const
{
    QList<QVariantMap> entries;
    for (const HoldSpec &hold : m_holds) {
        QVariantMap entry;
        entry.insert(QStringLiteral("Profile"), QVariant(hold.profile));
        entry.insert(QStringLiteral("Application"), QVariant(hold.application));
        entry.insert(QStringLiteral("Reason"), QVariant(hold.reason));
        entry.insert(QStringLiteral("AppId"), QVariant(hold.application));
        entries.push_back(entry);
    }
    return arrayOfStringVariantMaps(entries);
}

QString FakePpdService::introspect(const QString &path) const
{
    Q_UNUSED(path)
    return QStringLiteral("<node/>");
}

void FakePpdService::sendError(const QDBusMessage &message, const QString &name,
                               const QString &text)
{
    m_connection.send(message.createErrorReply(name, text));
}

bool FakePpdService::handleMessage(const QDBusMessage &message,
                                   const QDBusConnection &connection)
{
    Q_UNUSED(connection)
    if (message.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    const QString member = message.member();

    if (message.interface() == interfaceName()) {
        if (member == QStringLiteral("HoldProfile")) {
            if (message.arguments().size() != 4) {
                sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                          QStringLiteral("Bad HoldProfile"));
                return true;
            }
            HoldRequest request;
            request.profile = message.arguments().at(0).toString();
            request.reason = message.arguments().at(1).toString();
            request.application = message.arguments().at(2).toString();
            request.appId = message.arguments().at(3).toString();
            request.holdPath = QString::fromLatin1(kObjectPath) + QStringLiteral("/hold%1")
                                                  .arg(nextHoldNumber++);
            holdRequests.push_back(request);
            HoldSpec spec;
            spec.profile = request.profile;
            spec.application = request.application;
            spec.reason = request.reason;
            m_holds.push_back(spec);
            QDBusMessage reply = message.createReply();
            reply.setArguments({QVariant::fromValue(QDBusObjectPath(request.holdPath))});
            m_connection.send(reply);
            return true;
        }
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                  QStringLiteral("Unknown member ") + member);
        return true;
    }
    if (message.interface() == QString::fromLatin1(kPropertiesInterface)) {
        if (message.arguments().size() != 1) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                      QStringLiteral("Bad Properties call"));
            return true;
        }
        const QString requestedInterface =
            message.arguments().constFirst().toString();
        if (requestedInterface != interfaceName()) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface"),
                      QStringLiteral("Unknown interface ") + requestedInterface);
            return true;
        }
        if (member == QStringLiteral("GetAll")) {
            QVariantMap properties;
            properties.insert(QStringLiteral("ActiveProfile"), QVariant(m_activeProfile));
            properties.insert(QStringLiteral("Profiles"), profilesValue());
            properties.insert(holdsKey(), holdsValue());
            QDBusMessage reply = message.createReply();
            reply.setArguments({QVariant(properties)});
            m_connection.send(reply);
            return true;
        }
        if (member == QStringLiteral("Set")) {
            if (message.arguments().size() != 3
                || message.arguments().at(1).toString()
                       != QStringLiteral("ActiveProfile")) {
                sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                          QStringLiteral("Only ActiveProfile is writable"));
                return true;
            }
            if (m_rejectSetProfile) {
                sendError(message,
                          QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                          QStringLiteral("Rejected by fake policy"));
                return true;
            }
            setProfileRequests.push_back(
                message.arguments().at(2).value<QDBusVariant>().variant().toString());
            QDBusMessage reply = message.createReply();
            m_connection.send(reply);
            return true;
        }
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                  QStringLiteral("Unknown Properties member ") + member);
        return true;
    }
    if (message.path().startsWith(QString::fromLatin1(kObjectPath)
                                  + QStringLiteral("/hold"))
        && member == QStringLiteral("Release")) {
        if (!message.interface().isEmpty()
            && message.interface() != interfaceName()) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                      QStringLiteral("Wrong hold interface"));
            return true;
        }
        releaseRequests.push_back(message.path());
        QDBusMessage reply = message.createReply();
        m_connection.send(reply);
        return true;
    }
    sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
              QStringLiteral("Unsupported call"));
    return true;
}

} // namespace QindaQt::Tests
