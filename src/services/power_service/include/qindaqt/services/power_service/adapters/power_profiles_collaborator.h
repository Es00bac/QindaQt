// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: Production profile-authority adapter for power-profiles-daemon
// (bus names org.freedesktop.UPower.PowerProfiles with net.hadess.PowerProfiles
// legacy fallback) on an injected bus connection. The modern and legacy
// names use distinct standard object paths. Profile mutation goes through
// the standard Properties.Set("ActiveProfile") and HoldProfile()/Release()
// methods with no polkit interaction. HoldProfile returns a uint cookie and
// ReleaseProfile consumes that cookie on the daemon root object. Public hold
// identity is a one-way derivation of
// the upstream (profile, application, reason) tuple, never a raw object path;
// only holds acquired through this adapter are releasable, because upstream
// exposes no cookie for foreign holds. The adapter resolves the primary
// interface first and falls back to the legacy interface once per run.
class PowerProfilesCollaborator final : public ProfileCollaborator
{
    Q_OBJECT

public:
    explicit PowerProfilesCollaborator(const QDBusConnection &upstreamConnection,
                                       QObject *parent = nullptr);
    ~PowerProfilesCollaborator() override;

    quint64 start() override;
    void stop() override;
    void submitSetProfile(quint64 operationId, const QString &profileId) override;
    void submitAcquireProfileHold(quint64 operationId, const QString &profileId,
                                  const QString &applicationName,
                                  const QString &reason) override;
    void submitReleaseProfileHold(quint64 operationId, const Handle &hold) override;

private:
    void scheduleUnavailable(quint64 generation, const QString &reasonCode);
    void refreshFacts(quint64 generation);
    void tryRefreshCandidate(quint64 generation, quint64 refreshSerial, int index);
    void applyProperties(quint64 generation, const QVariantMap &properties);
    void finishOperation(quint64 generation, quint64 operationId,
                         CollaboratorStatus status, const QString &reasonCode);
    void onProfileOwnerChanged(const QString &name, const QString &oldOwner,
                               const QString &newOwner);
    void callRelease(quint32 cookie, quint64 generation,
                     quint64 operationId, const QString &ownerAtSubmission);
    void adoptOwner(const QString &owner);
    [[nodiscard]] bool runningGeneration(quint64 generation) const;

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_primaryWatcher = nullptr;
    QDBusServiceWatcher *m_legacyWatcher = nullptr;
    QHash<QString, quint32> m_acquiredHolds;
    QString m_primaryOwner;
    QString m_legacyOwner;
    QString m_activeOwner;
    QString m_activeServiceName;
    QString m_activeInterfaceName;
    QString m_activeObjectPath;
    QString m_lastOwner;
    quint64 m_generation = 0;
    quint64 m_nextGeneration = 0;
    quint64 m_refreshSerial = 0;
    bool m_running = false;
    bool m_signalsSubscribed = false;
    bool m_epochAdvancedForLoss = false;

private Q_SLOTS:
    void onPpdPropertiesChanged(const QDBusMessage &message);
};

} // namespace QindaQt::Power::Upstream
