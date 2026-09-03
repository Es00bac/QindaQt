// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/power_profiles_collaborator.h>

#include "upstream_dbus_util.h"
#include "upstream_identity.h"

#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusReply>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kPrimaryName[] = "org.freedesktop.UPower.PowerProfiles";
constexpr char kPrimaryPath[] = "/org/freedesktop/UPower/PowerProfiles";
constexpr char kPrimaryInterface[] = "org.freedesktop.UPower.PowerProfiles";
constexpr char kLegacyName[] = "net.hadess.PowerProfiles";
constexpr char kLegacyPath[] = "/net/hadess/PowerProfiles";
constexpr char kLegacyInterface[] = "net.hadess.PowerProfiles";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

struct Candidate {
    const char *service;
    const char *path;
    const char *interface;
};

constexpr Candidate kCandidates[] = {
    {kPrimaryName, kPrimaryPath, kPrimaryInterface},
    {kLegacyName, kLegacyPath, kLegacyInterface},
};

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

bool requiredStringEntry(const QVariantMap &entry, const QString &key,
                         QString &value)
{
    const auto it = entry.constFind(key);
    if (it == entry.constEnd() || it.value().metaType() != QMetaType::fromType<QString>()) {
        return false;
    }
    value = it.value().toString();
    return !value.isEmpty();
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
    if (++m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_acquiredHolds.clear();
    m_primaryOwner.clear();
    m_legacyOwner.clear();
    m_activeOwner.clear();
    m_lastOwner.clear();
    m_activeServiceName.clear();
    m_activeInterfaceName.clear();
    m_activeObjectPath.clear();
    m_epochAdvancedForLoss = false;

    if (!m_connection.isConnected()) {
        scheduleUnavailable(m_generation, QStringLiteral("profiles-bus-unavailable"));
        return m_generation;
    }
    const QDBusReply<QString> primaryOwner =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kPrimaryName));
    if (primaryOwner.isValid()) {
        m_primaryOwner = primaryOwner.value();
    }
    const QDBusReply<QString> legacyOwner =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kLegacyName));
    if (legacyOwner.isValid()) {
        m_legacyOwner = legacyOwner.value();
    }
    if (m_primaryWatcher == nullptr) {
        m_primaryWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(kPrimaryName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_primaryWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &PowerProfilesCollaborator::onProfileOwnerChanged);
    }
    if (m_legacyWatcher == nullptr) {
        m_legacyWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(kLegacyName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_legacyWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &PowerProfilesCollaborator::onProfileOwnerChanged);
    }
    if (!m_signalsSubscribed
        && !m_connection.connect(
            QString(), QString(), QString::fromLatin1(kPropertiesInterface),
            QStringLiteral("PropertiesChanged"), this,
            SLOT(onPpdPropertiesChanged(QDBusMessage)))) {
        scheduleUnavailable(m_generation, QStringLiteral("profiles-unavailable"));
        return m_generation;
    }
    m_signalsSubscribed = true;
    refreshFacts(m_generation);
    return m_generation;
}

void PowerProfilesCollaborator::stop()
{
    m_running = false;
    ++m_refreshSerial;
    m_acquiredHolds.clear();
    m_activeOwner.clear();
    m_activeServiceName.clear();
    m_activeInterfaceName.clear();
    m_activeObjectPath.clear();
}

bool PowerProfilesCollaborator::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

void PowerProfilesCollaborator::scheduleUnavailable(
    const quint64 generation, const QString &reasonCode)
{
    QTimer::singleShot(0, this, [this, generation, reasonCode]() {
        if (runningGeneration(generation)) {
            Q_EMIT statusUnavailable(generation, reasonCode);
        }
    });
}

void PowerProfilesCollaborator::onPpdPropertiesChanged(
    const QDBusMessage &message)
{
    if (!m_running || message.arguments().isEmpty()) {
        return;
    }
    const QString interfaceName = message.arguments().constFirst().toString();
    const bool primary = message.path() == QString::fromLatin1(kPrimaryPath)
        && interfaceName == QString::fromLatin1(kPrimaryInterface);
    const bool legacy = message.path() == QString::fromLatin1(kLegacyPath)
        && interfaceName == QString::fromLatin1(kLegacyInterface);
    if (primary || legacy) {
        refreshFacts(m_generation);
    }
}

void PowerProfilesCollaborator::refreshFacts(const quint64 generation)
{
    if (++m_refreshSerial == 0) {
        ++m_refreshSerial;
    }
    tryRefreshCandidate(generation, m_refreshSerial, 0);
}

void PowerProfilesCollaborator::tryRefreshCandidate(
    const quint64 generation, const quint64 refreshSerial, const int index)
{
    if (!runningGeneration(generation) || refreshSerial != m_refreshSerial) {
        return;
    }
    if (index >= static_cast<int>(std::size(kCandidates))) {
        scheduleUnavailable(generation, QStringLiteral("profiles-unavailable"));
        return;
    }
    const Candidate candidate = kCandidates[index];
    getAllProperties(
        m_connection, QString::fromLatin1(candidate.service),
        QString::fromLatin1(candidate.path), QString::fromLatin1(candidate.interface),
        this,
        [this, generation, refreshSerial, candidate](
            const QVariantMap &properties, const QString &owner) {
            if (!runningGeneration(generation) || refreshSerial != m_refreshSerial
                || owner.isEmpty()) {
                return;
            }
            adoptOwner(owner);
            m_activeServiceName = QString::fromLatin1(candidate.service);
            m_activeInterfaceName = QString::fromLatin1(candidate.interface);
            m_activeObjectPath = QString::fromLatin1(candidate.path);
            applyProperties(generation, properties);
        },
        [this, generation, refreshSerial, index](const QString &) {
            tryRefreshCandidate(generation, refreshSerial, index + 1);
        });
}

void PowerProfilesCollaborator::applyProperties(
    const quint64 generation, const QVariantMap &properties)
{
    const auto profilesIt = properties.constFind(QStringLiteral("Profiles"));
    const auto activeIt = properties.constFind(QStringLiteral("ActiveProfile"));
    if (profilesIt == properties.constEnd() || activeIt == properties.constEnd()
        || activeIt.value().metaType() != QMetaType::fromType<QString>()) {
        scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
        return;
    }

    QList<QVariantMap> profiles;
    if (!readStringVariantMapArray(profilesIt.value(), profiles)) {
        scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
        return;
    }
    ProfileFacts facts;
    for (const QVariantMap &entry : profiles) {
        QString profileId;
        if (!requiredStringEntry(entry, QStringLiteral("Profile"), profileId)) {
            scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
            return;
        }
        facts.profiles.supported.push_back(
            Profile{.id = profileId, .label = canonicalProfileLabel(profileId)});
    }
    facts.profiles.activeProfileId = activeIt.value().toString();

    auto holdsIt = properties.constFind(QStringLiteral("ActiveProfileHolds"));
    if (holdsIt == properties.constEnd()
        && m_activeInterfaceName == QString::fromLatin1(kLegacyInterface)) {
        holdsIt = properties.constFind(QStringLiteral("Holds"));
    }
    if (holdsIt != properties.constEnd()) {
        QList<QVariantMap> holds;
        if (!readStringVariantMapArray(holdsIt.value(), holds)) {
            scheduleUnavailable(generation, QStringLiteral("profiles-malformed"));
            return;
        }
        for (const QVariantMap &entry : holds) {
            QString profileId;
            QString applicationId;
            QString reason;
            if (!requiredStringEntry(entry, QStringLiteral("Profile"), profileId)
                || !requiredStringEntry(entry, QStringLiteral("ApplicationId"),
                                        applicationId)
                || !requiredStringEntry(entry, QStringLiteral("Reason"), reason)) {
                scheduleUnavailable(generation,
                                    QStringLiteral("profiles-malformed"));
                return;
            }
            ProfileHold hold;
            hold.handle.opaqueId = deriveOpaqueId(
                QStringLiteral("profile-hold"),
                profileId + QLatin1Char('|') + applicationId + QLatin1Char('|')
                    + reason);
            hold.profileId = profileId;
            hold.applicationName = applicationId;
            hold.reason = reason;
            facts.profiles.holds.push_back(std::move(hold));
        }
    }
    Q_EMIT factsChanged(generation, facts);
}

void PowerProfilesCollaborator::finishOperation(
    const quint64 generation, const quint64 operationId,
    const CollaboratorStatus status, const QString &reasonCode)
{
    Q_EMIT operationFinished(
        generation, operationId,
        CollaboratorOutcome{.status = status,
                            .reasonCode = reasonCode,
                            .diagnostic = {}});
}

void PowerProfilesCollaborator::submitSetProfile(
    const quint64 operationId, const QString &profileId)
{
    if (m_activeOwner.isEmpty() || m_activeServiceName.isEmpty()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profiles-unavailable"));
        return;
    }
    const quint64 generation = m_generation;
    const QString owner = m_activeOwner;
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, m_activeObjectPath, QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("Set"));
    call.setArguments({m_activeInterfaceName, QStringLiteral("ActiveProfile"),
                       QVariant::fromValue(QDBusVariant(profileId))});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, owner]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (m_activeOwner != owner) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                } else if (reply.type() != QDBusMessage::ReplyMessage) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                } else {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Succeeded,
                                    QStringLiteral("applied"));
                }
            });
}

void PowerProfilesCollaborator::submitAcquireProfileHold(
    const quint64 operationId, const QString &profileId,
    const QString &applicationName, const QString &reason)
{
    if (m_activeOwner.isEmpty() || m_activeServiceName.isEmpty()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profiles-unavailable"));
        return;
    }
    const quint64 generation = m_generation;
    const QString owner = m_activeOwner;
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, m_activeObjectPath, m_activeInterfaceName,
        QStringLiteral("HoldProfile"));
    call.setArguments({profileId, reason, applicationName});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, profileId, applicationName,
             reason, owner]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (m_activeOwner != owner) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().size() != 1
                    || reply.arguments().constFirst().metaType()
                        != QMetaType::fromType<uint>()) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                    return;
                }
                const quint32 cookie = reply.arguments().constFirst().toUInt();
                const QString opaqueId = deriveOpaqueId(
                    QStringLiteral("profile-hold"),
                    profileId + QLatin1Char('|') + applicationName
                        + QLatin1Char('|') + reason);
                m_acquiredHolds.insert(opaqueId, cookie);
                finishOperation(generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

void PowerProfilesCollaborator::submitReleaseProfileHold(
    const quint64 operationId, const Handle &hold)
{
    const auto it = m_acquiredHolds.constFind(hold.opaqueId);
    if (it == m_acquiredHolds.constEnd()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("hold-not-releasable"));
        return;
    }
    callRelease(it.value(), m_generation, operationId, m_activeOwner);
}

void PowerProfilesCollaborator::callRelease(
    const quint32 cookie, const quint64 generation, const quint64 operationId,
    const QString &ownerAtSubmission)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        ownerAtSubmission, m_activeObjectPath, m_activeInterfaceName,
        QStringLiteral("ReleaseProfile"));
    call.setArguments({cookie});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, cookie, ownerAtSubmission]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (m_activeOwner != ownerAtSubmission) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                if (reply.type() != QDBusMessage::ReplyMessage) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                    return;
                }
                for (auto it = m_acquiredHolds.begin();
                     it != m_acquiredHolds.end(); ++it) {
                    if (it.value() == cookie) {
                        m_acquiredHolds.erase(it);
                        break;
                    }
                }
                finishOperation(generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

void PowerProfilesCollaborator::adoptOwner(const QString &owner)
{
    const bool changed = !m_lastOwner.isEmpty() && m_lastOwner != owner;
    m_activeOwner = owner;
    m_lastOwner = owner;
    if (changed && !m_epochAdvancedForLoss) {
        Q_EMIT authorityReplaced(m_generation);
    }
    m_epochAdvancedForLoss = false;
}

void PowerProfilesCollaborator::onProfileOwnerChanged(
    const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    const QDBusReply<QString> resolved =
        m_connection.interface()->serviceOwner(name);
    const QString currentOwner = resolved.isValid() ? resolved.value() : QString();
    if (name == QString::fromLatin1(kPrimaryName)) {
        m_primaryOwner = currentOwner;
    } else if (name == QString::fromLatin1(kLegacyName)) {
        m_legacyOwner = currentOwner;
    }
    if (!m_running) {
        return;
    }

    ++m_refreshSerial;
    const QString nextOwner = !m_primaryOwner.isEmpty() ? m_primaryOwner
                                                        : m_legacyOwner;
    if (nextOwner.isEmpty()) {
        if (!m_activeOwner.isEmpty()) {
            m_epochAdvancedForLoss = true;
            Q_EMIT authorityReplaced(m_generation);
        }
        m_activeOwner.clear();
        m_activeServiceName.clear();
        m_activeInterfaceName.clear();
        m_activeObjectPath.clear();
        m_acquiredHolds.clear();
        Q_EMIT statusUnavailable(m_generation,
                                 QStringLiteral("profiles-unavailable"));
        return;
    }
    const bool replacement = !m_activeOwner.isEmpty()
        && m_activeOwner != nextOwner;
    adoptOwner(nextOwner);
    m_acquiredHolds.clear();
    if (replacement) {
        Q_EMIT statusUnavailable(m_generation,
                                 QStringLiteral("profiles-unavailable"));
    }
    refreshFacts(m_generation);
}

} // namespace QindaQt::Power::Upstream
