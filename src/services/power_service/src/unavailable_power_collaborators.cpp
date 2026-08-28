// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/unavailable_power_collaborators.h>

#include <QtCore/QTimer>

#include <functional>

namespace QindaQt::Power {
namespace {

// The unavailable publication must be asynchronous so the resident service
// finishes name registration before its first invalidated signal can race a
// client subscription on the same turn.
void scheduleUnavailable(QObject *owner, const std::function<void()> &emitOnce)
{
    QTimer::singleShot(0, owner, emitOnce);
}

} // namespace

quint64 UnavailableBatteryCollaborator::start()
{
    static quint64 generation = 0;
    ++generation;
    const quint64 run = generation;
    scheduleUnavailable(this, [this, run] {
        Q_EMIT statusUnavailable(run, QString::fromLatin1(kUpstreamNotIntegratedReason));
    });
    return run;
}

void UnavailableBatteryCollaborator::stop()
{
}

void UnavailableBatteryCollaborator::submitSetKeyboardBrightness(
    const quint64 operationId, const Handle &device, const quint32 value)
{
    Q_UNUSED(device)
    Q_UNUSED(value)
    Q_EMIT operationFinished(
        0, operationId,
        CollaboratorOutcome{.status = CollaboratorStatus::Unsupported,
                            .reasonCode = QString::fromLatin1(
                                kUpstreamNotIntegratedReason),
                            .diagnostic = {}});
}

quint64 UnavailableProfileCollaborator::start()
{
    static quint64 generation = 0;
    ++generation;
    const quint64 run = generation;
    scheduleUnavailable(this, [this, run] {
        Q_EMIT statusUnavailable(run, QString::fromLatin1(kUpstreamNotIntegratedReason));
    });
    return run;
}

void UnavailableProfileCollaborator::stop()
{
}

void UnavailableProfileCollaborator::submitSetProfile(const quint64 operationId,
                                                      const QString &profileId)
{
    Q_UNUSED(profileId)
    Q_EMIT operationFinished(
        0, operationId,
        CollaboratorOutcome{.status = CollaboratorStatus::Unsupported,
                            .reasonCode = QString::fromLatin1(
                                kUpstreamNotIntegratedReason),
                            .diagnostic = {}});
}

void UnavailableProfileCollaborator::submitAcquireProfileHold(
    const quint64 operationId, const QString &profileId,
    const QString &applicationName, const QString &reason)
{
    Q_UNUSED(profileId)
    Q_UNUSED(applicationName)
    Q_UNUSED(reason)
    Q_EMIT operationFinished(
        0, operationId,
        CollaboratorOutcome{.status = CollaboratorStatus::Unsupported,
                            .reasonCode = QString::fromLatin1(
                                kUpstreamNotIntegratedReason),
                            .diagnostic = {}});
}

void UnavailableProfileCollaborator::submitReleaseProfileHold(
    const quint64 operationId, const Handle &hold)
{
    Q_UNUSED(hold)
    Q_EMIT operationFinished(
        0, operationId,
        CollaboratorOutcome{.status = CollaboratorStatus::Unsupported,
                            .reasonCode = QString::fromLatin1(
                                kUpstreamNotIntegratedReason),
                            .diagnostic = {}});
}

quint64 UnavailableSessionCollaborator::start()
{
    static quint64 generation = 0;
    ++generation;
    const quint64 run = generation;
    scheduleUnavailable(this, [this, run] {
        Q_EMIT statusUnavailable(run, QString::fromLatin1(kUpstreamNotIntegratedReason));
    });
    return run;
}

void UnavailableSessionCollaborator::stop()
{
}

} // namespace QindaQt::Power
