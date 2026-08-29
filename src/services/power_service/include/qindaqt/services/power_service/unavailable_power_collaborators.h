// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

namespace QindaQt::Power {

// AGENT-NOTE: PB-1 ships no host UPower, power-profiles-daemon, or logind
// transport. These deterministic collaborators make the resident process
// publish an honest protocol-valid Unavailable snapshot with zero
// capabilities instead of touching host services or fabricating battery
// truth. Real adapters replace them in later slices without coordinator
// changes; the reason token is part of that contract.
inline constexpr char kUpstreamNotIntegratedReason[] = "upstream-not-integrated";

class UnavailableBatteryCollaborator final : public BatteryCollaborator
{
    Q_OBJECT

public:
    using BatteryCollaborator::BatteryCollaborator;

    quint64 start() override;
    void stop() override;
    void submitSetKeyboardBrightness(quint64 operationId, const Handle &device,
                                     quint32 value) override;
};

class UnavailableProfileCollaborator final : public ProfileCollaborator
{
    Q_OBJECT

public:
    using ProfileCollaborator::ProfileCollaborator;

    quint64 start() override;
    void stop() override;
    void submitSetProfile(quint64 operationId, const QString &profileId) override;
    void submitAcquireProfileHold(quint64 operationId, const QString &profileId,
                                  const QString &applicationName,
                                  const QString &reason) override;
    void submitReleaseProfileHold(quint64 operationId, const Handle &hold) override;
};

class UnavailableSessionCollaborator final : public SessionCollaborator
{
    Q_OBJECT

public:
    using SessionCollaborator::SessionCollaborator;

    quint64 start() override;
    void stop() override;
};

} // namespace QindaQt::Power
