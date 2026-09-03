// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/power_profiles_collaborator.h>

#include "upstream_dbus_util.h"
#include "upstream_identity.h"

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kPpdPrimaryName[] = "org.freedesktop.UPower.PowerProfiles";
constexpr char kPpdLegacyName[] = "net.hadess.PowerProfiles";
constexpr char kPpdObjectPath[] = "/net/hadess/PowerProfiles";
constexpr char kPpdPrimaryInterface[] = "org.freedesktop.UPower.PowerProfiles";
constexpr char kPpdLegacyInterface[] = "net.hadess.PowerProfiles";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

QString canonicalProfileLabel(const QString &profileId)
{
    if (profileId == QStringLiteral("power-saver")) {
        return QStringLiteral("Power Saver");
    }
    if (profileId == QStringLiteral("balanced")) {
        return QStringLiteral("Balanced");
    }
    if (profileId == QStringLiteral("performance")) {
        return QStringLiteral("Performance");
    }
    return profileId;
}

bool optionalStringEntry(const QVariantMap &entry, const QString &key,
                         QString &value)
{
    const auto it = entry.constFind(key);
    if (it == entry.constEnd()) {
        value = QString();
        return true;
    }
    if (it.value().userType() != QMetaType::QString) {
        return false;
    }
    value = it.value().toString();
    return true;
}

} // namespace

PowerProfilesCollaborator::PowerProfilesCollaborator(
    const QDBusConnection &upstreamConnection, QObject *parent)
    : ProfileCollaborator(parent)
    , m_connection(upstreamConnection)
{
}

PowerProfilesCollaborator::~PowerProfilesCollaborator()
{
    stop();
}

quint64 PowerProfilesCollaborator::start()
{
    ++m_nextGeneration;
    if (m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_acquiredHolds.clear();
    m_activeOwner.clear();
    m_activeServiceName.clear();
    m_activeInterfaceName.clear();

    if (!m_connection.isConnected()) {
        scheduleUnavailable(m_generation, QStringLiteral("profiles-bus-unavailable"));
        return m_generation;
    }
    if (m_primaryWatcher == nullptr) {
        m_primaryWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(kPpdPrimaryName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_primaryWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &PowerProfilesCollaborator::onProfileOwnerChanged);
    }
    if (m_legacyWatcher == nullptr) {
        m_legacyWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(kPpdLegacyName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_legacyWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &PowerProfilesCollaborator::onProfileOwnerChanged);
    }
    // One subscription covers both daemon interface generations; the slot
    // accepts either interface name carried by the signal.
    if (!m_connection.connect(
            QString(), QString::fromLatin1(kPpdObjectPath),
            QString::fromLatin1(kPropertiesInterface),
            QStringLiteral("PropertiesChanged"), this,
            SLOT(onPpdPropertiesChanged(QDBusMessage)))) {
        scheduleUnavailable(m_generation, QStringLiteral("profiles-unavailable"));
        return m_generation;
    }
    refreshFacts(m_generation);
    return m_generation;
}

void PowerProfilesCollaborator::stop()
{
    // Signal hooks stay registered for this object's lifetime; they are
    // generation-guarded and QtDBus drops them at destruction.
    m_running = false;
    m_acquiredHolds.clear();
    m_activeOwner.clear();
    m_activeServiceName.clear();
    m_activeInterfaceName.clear();
}

bool PowerProfilesCollaborator::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

void PowerProfilesCollaborator::scheduleUnavailable(const quint64 generation,
                                                    const QString &reasonCode)
{
    QTimer::singleShot(0, this, [this, generation, reasonCode]() {
        if (!runningGeneration(generation)) {
            return;
        }
        Q_EMIT statusUnavailable(generation, reasonCode);
    });
}

void PowerProfilesCollaborator::onPpdPropertiesChanged(const QDBusMessage &message)
{
    if (message.arguments().isEmpty()) {
        return;
    }
    const QString interfaceName = message.arguments().at(0).toString();
    if (interfaceName != QString::fromLatin1(kPpdPrimaryInterface)
        && interfaceName != QString::fromLatin1(kPpdLegacyInterface)) {
        return;
    }
    if (runningGeneration(m_generation)) {
        refreshFacts(m_generation);
    }
}

void PowerProfilesCollaborator::refreshFacts(const quint64 generation)
{
    tryRefreshCandidate(generation, 0);
}

void PowerProfilesCollaborator::tryRefreshCandidate(const quint64 generation,
                                                    const int index)
{
    // AGENT-GUARD: candidate order is part of the adapter contract -- primary
    // bus name and interface first, legacy fallbacks after. Each candidate is
    // tried only while the run is current, so a restart never adopts a stale
    // authority.
    struct Candidate {
        const char *serviceName;
        const char *interfaceName;
    };
    static const Candidate candidates[] = {
        {kPpdPrimaryName, kPpdPrimaryInterface},
        {kPpdPrimaryName, kPpdLegacyInterface},
        {kPpdLegacyName, kPpdPrimaryInterface},
        {kPpdLegacyName, kPpdLegacyInterface}};

    if (index >= 4) {
        scheduleUnavailable(generation, QStringLiteral("profiles-unavailable"));
        return;
    }
    const Candidate &candidate = candidates[index];
    Upstream::getAllProperties(
        m_connection, QString::fromLatin1(candidate.serviceName),
        QString::fromLatin1(kPpdObjectPath),
        QString::fromLatin1(candidate.interfaceName), this,
        [this, generation, candidate](const QVariantMap &properties,
                                      const QString &sender) {
            if (!runningGeneration(generation)) {
                return;
            }
            m_activeServiceName = QString::fromLatin1(candidate.serviceName);
            m_activeInterfaceName = QString::fromLatin1(candidate.interfaceName);
            if (!sender.isEmpty()) {
                m_activeOwner = sender;
            }
            applyProperties(generation, properties);
        },
        [this, generation, index](const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            tryRefreshCandidate(generation, index + 1);
        });
}

void PowerProfilesCollaborator::applyProperties(const quint64 generation,
                                                const QVariantMap &properties)
{
    ProfileFacts facts;
    if (properties.contains(QStringLiteral("Profiles"))) {
        QList<QVariantMap> entries;
        if (!Upstream::readStringVariantMapArray(
                properties.value(QStringLiteral("Profiles")), entries)) {
            scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
            return;
        }
        for (const QVariantMap &entry : entries) {
            QString profileId;
            if (!optionalStringEntry(entry, QStringLiteral("Profile"), profileId)) {
                scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
                return;
            }
            Profile profile;
            profile.id = profileId;
            profile.label = canonicalProfileLabel(profileId);
            facts.profiles.supported.push_back(std::move(profile));
        }
    }
    if (properties.contains(QStringLiteral("ActiveProfile"))) {
        const QVariant &active =
            properties.value(QStringLiteral("ActiveProfile"));
        if (active.userType() != QMetaType::QString) {
            scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
            return;
        }
        facts.profiles.activeProfileId = active.toString();
    }
    const QString holdsKey = properties.contains(
                                 QStringLiteral("ActiveProfileHolds"))
        ? QStringLiteral("ActiveProfileHolds")
        : QStringLiteral("Holds");
    if (properties.contains(holdsKey)) {
        QList<QVariantMap> entries;
        if (!Upstream::readStringVariantMapArray(properties.value(holdsKey),
                                                 entries)) {
            scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
            return;
        }
        for (const QVariantMap &entry : entries) {
            QString profileId;
            QString application;
            QString reason;
            if (!optionalStringEntry(entry, QStringLiteral("Profile"), profileId)
                || !optionalStringEntry(entry, QStringLiteral("Application"),
                                        application)
                || !optionalStringEntry(entry, QStringLiteral("Reason"), reason)) {
                scheduleUnavailable(generation,
                                    QStringLiteral("profiles-malformed"));
                return;
            }
            ProfileHold hold;
            hold.handle.opaqueId = Upstream::deriveOpaqueId(
                QStringLiteral("profile-hold"),
                profileId + QLatin1Char('|') + application + QLatin1Char('|')
                    + reason);
            hold.profileId = profileId;
            hold.applicationName = application;
            hold.reason = reason;
            facts.profiles.holds.push_back(std::move(hold));
        }
    }
    Q_EMIT factsChanged(generation, facts);
}

void PowerProfilesCollaborator::finishOperation(const quint64 generation,
                                                const quint64 operationId,
                                                const CollaboratorStatus status,
                                                const QString &reasonCode)
{
    Q_EMIT operationFinished(
        generation, operationId,
        CollaboratorOutcome{.status = status,
                            .reasonCode = reasonCode,
                            .diagnostic = {}});
}

void PowerProfilesCollaborator::submitSetProfile(const quint64 operationId,
                                                 const QString &profileId)
{
    if (m_activeServiceName.isEmpty() || m_activeInterfaceName.isEmpty()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profiles-unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        m_activeServiceName, QString::fromLatin1(kPpdObjectPath),
        QString::fromLatin1(kPropertiesInterface), QStringLiteral("Set"));
    call.setArguments({m_activeInterfaceName, QStringLiteral("ActiveProfile"),
                       QVariant::fromValue(QDBusVariant(profileId))});
    const QString ownerAtSubmission = m_activeOwner;
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, operationId, ownerAtSubmission]() {
                watcher->deleteLater();
                if (!m_running) {
                    return;
                }
                if (watcher->reply().type() != QDBusMessage::ReplyMessage) {
                    finishOperation(m_generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                    return;
                }
                if (!ownerAtSubmission.isEmpty()
                    && m_activeOwner != ownerAtSubmission) {
                    finishOperation(m_generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                finishOperation(m_generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

void PowerProfilesCollaborator::submitAcquireProfileHold(
    const quint64 operationId, const QString &profileId,
    const QString &applicationName, const QString &reason)
{
    if (m_activeServiceName.isEmpty() || m_activeInterfaceName.isEmpty()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profiles-unavailable"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        m_activeServiceName, QString::fromLatin1(kPpdObjectPath),
        m_activeInterfaceName, QStringLiteral("HoldProfile"));
    call.setArguments({profileId, reason, applicationName, applicationName});
    const QString ownerAtSubmission = m_activeOwner;
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, operationId, profileId, applicationName, reason,
             ownerAtSubmission]() {
                watcher->deleteLater();
                if (!m_running) {
                    return;
                }
                const QDBusMessage reply = watcher->reply();
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().isEmpty()) {
                    finishOperation(m_generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                    return;
                }
                if (!ownerAtSubmission.isEmpty()
                    && m_activeOwner != ownerAtSubmission) {
                    finishOperation(m_generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                const QVariant holdArgument = reply.arguments().constFirst();
                if (holdArgument.userType()
                    != QMetaType::fromType<QDBusObjectPath>().id()) {
                    finishOperation(m_generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-malformed"));
                    return;
                }
                const QString holdPath =
                    holdArgument.value<QDBusObjectPath>().path();
                if (holdPath.isEmpty() || !holdPath.startsWith(QLatin1Char('/'))) {
                    finishOperation(m_generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-malformed"));
                    return;
                }
                const QString opaqueId = Upstream::deriveOpaqueId(
                    QStringLiteral("profile-hold"),
                    profileId + QLatin1Char('|') + applicationName
                        + QLatin1Char('|') + reason);
                m_acquiredHolds.insert(opaqueId, holdPath);
                finishOperation(m_generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

void PowerProfilesCollaborator::submitReleaseProfileHold(
    const quint64 operationId, const Handle &hold)
{
    const QString opaqueId = hold.opaqueId;
    // AGENT-GUARD: upstream exposes no object path for holds this process did
    // not acquire; releasing a foreign hold must fail closed rather than guess
    // a path.
    if (!m_acquiredHolds.contains(opaqueId)) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("hold-not-releasable"));
        return;
    }
    callRelease(m_acquiredHolds.value(opaqueId), m_generation, operationId,
                m_activeOwner);
}

void PowerProfilesCollaborator::callRelease(const QString &holdObjectPath,
                                            const quint64 generation,
                                            const quint64 operationId,
                                            const QString &ownerAtSubmission)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        m_activeServiceName, holdObjectPath, m_activeInterfaceName,
        QStringLiteral("Release"));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, holdObjectPath,
             ownerAtSubmission]() {
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (watcher->reply().type() != QDBusMessage::ReplyMessage) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                    return;
                }
                if (!ownerAtSubmission.isEmpty()
                    && m_activeOwner != ownerAtSubmission) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                // The acquired-hold table is keyed by tuple derivation, so
                // the completed hold is removed by its object-path value.
                for (auto it = m_acquiredHolds.begin();
                     it != m_acquiredHolds.end(); ++it) {
                    if (it.value() == holdObjectPath) {
                        m_acquiredHolds.erase(it);
                        break;
                    }
                }
                finishOperation(generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

void PowerProfilesCollaborator::onProfileOwnerChanged(const QString &name,
                                                      const QString &oldOwner,
                                                      const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (name == QString::fromLatin1(kPpdPrimaryName)) {
        m_primaryOwner = newOwner;
    } else {
        m_legacyOwner = newOwner;
    }
    if (!m_running) {
        return;
    }
    const QString nextActive = !m_primaryOwner.isEmpty() ? m_primaryOwner
                                                         : m_legacyOwner;
    const QString previousActive = m_activeOwner;
    m_acquiredHolds.clear();
    m_activeServiceName.clear();
    m_activeInterfaceName.clear();
    if (nextActive.isEmpty()) {
        m_activeOwner.clear();
        scheduleUnavailable(m_generation, QStringLiteral("profiles-unavailable"));
        return;
    }
    const bool replaced = !previousActive.isEmpty() && previousActive != nextActive;
    m_activeOwner = nextActive;
    if (replaced) {
        Q_EMIT authorityReplaced(m_generation);
    }
    refreshFacts(m_generation);
}

} // namespace QindaQt::Power::Upstream
