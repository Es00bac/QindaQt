// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_service/power_service_coordinator.h>

#include "power_service_assembly_p.h"

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtCore/QRandomGenerator>

#include <limits>
#include <utility>

namespace QindaQt::Power {
namespace {

constexpr quint64 kMaximumEpochJump = 1024;

} // namespace

PowerServiceCoordinator::PowerServiceCoordinator(BatteryCollaborator *battery,
                                                 ProfileCollaborator *profiles,
                                                 SessionCollaborator *session,
                                                 QObject *parent)
    : QObject(parent)
    , m_battery(battery)
    , m_profiles(profiles)
    , m_session(session)
{
    Q_ASSERT(m_battery != nullptr && m_profiles != nullptr && m_session != nullptr);
    quint64 epoch = QRandomGenerator::system()->generate64();
    if (epoch == 0) {
        epoch = 1;
    }
    m_snapshot.protocolVersion = kProtocolVersion;
    m_snapshot.epoch = epoch;
    m_snapshot.revision = 1;
    m_snapshot.availability = Availability::Starting;
    m_snapshot.reasonCode = QStringLiteral("starting");

    connectBattery(m_battery);
    connectProfile(m_profiles);
    connectSession(m_session);
}

const Snapshot &PowerServiceCoordinator::snapshot() const noexcept
{
    return m_snapshot;
}

void PowerServiceCoordinator::connectBattery(BatteryCollaborator *battery)
{
    connect(battery, &BatteryCollaborator::factsChanged, this,
            &PowerServiceCoordinator::acceptBatteryFacts);
    connect(battery, &BatteryCollaborator::statusUnavailable, this,
            &PowerServiceCoordinator::acceptBatteryUnavailable);
    connect(battery, &BatteryCollaborator::authorityReplaced, this,
            &PowerServiceCoordinator::acceptBatteryReplacement);
    connect(battery, &BatteryCollaborator::operationFinished, this,
            &PowerServiceCoordinator::acceptBatteryOutcome);
}

void PowerServiceCoordinator::connectProfile(ProfileCollaborator *profiles)
{
    connect(profiles, &ProfileCollaborator::factsChanged, this,
            &PowerServiceCoordinator::acceptProfileFacts);
    connect(profiles, &ProfileCollaborator::statusUnavailable, this,
            &PowerServiceCoordinator::acceptProfileUnavailable);
    connect(profiles, &ProfileCollaborator::authorityReplaced, this,
            &PowerServiceCoordinator::acceptProfileReplacement);
    connect(profiles, &ProfileCollaborator::operationFinished, this,
            &PowerServiceCoordinator::acceptProfileOutcome);
}

void PowerServiceCoordinator::connectSession(SessionCollaborator *session)
{
    connect(session, &SessionCollaborator::factsChanged, this,
            &PowerServiceCoordinator::acceptSessionFacts);
    connect(session, &SessionCollaborator::statusUnavailable, this,
            &PowerServiceCoordinator::acceptSessionUnavailable);
    connect(session, &SessionCollaborator::authorityReplaced, this,
            &PowerServiceCoordinator::acceptSessionReplacement);
}

bool PowerServiceCoordinator::generationCurrent(const DomainState &state,
                                                const quint64 generation) const
{
    // AGENT-GUARD: A stopped or superseded collaborator run must never mutate
    // authoritative state. Generation zero and non-current runs are dropped
    // before any validation or publication work.
    return m_running && generation != 0 && generation == state.generation;
}

QString PowerServiceCoordinator::sanitizeReasonCode(const QString &reasonCode) const
{
    const QString sanitized = sanitizeText(reasonCode, kMaxReasonCodeUtf8Bytes);
    if (sanitized.isEmpty()) {
        return QStringLiteral("upstream-unavailable");
    }
    for (const QChar character : sanitized) {
        const bool lowerAscii = character >= QLatin1Char('a')
            && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
        if (!lowerAscii && !digit && character != QLatin1Char('-')) {
            return QStringLiteral("upstream-malformed");
        }
    }
    return sanitized;
}

void PowerServiceCoordinator::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_batteryDomain.state.generation = m_battery->start();
    m_profileDomain.state.generation = m_profiles->start();
    m_sessionDomain.state.generation = m_session->start();
    if (m_batteryDomain.state.generation == 0) {
        m_batteryDomain.has = true;
        markDomainUnavailable(m_batteryDomain.state, 0,
                              QStringLiteral("collaborator-start-failed"));
    }
    if (m_profileDomain.state.generation == 0) {
        m_profileDomain.has = true;
        markDomainUnavailable(m_profileDomain.state, 0,
                              QStringLiteral("collaborator-start-failed"));
    }
    if (m_sessionDomain.state.generation == 0) {
        m_sessionDomain.has = true;
        markDomainUnavailable(m_sessionDomain.state, 0,
                              QStringLiteral("collaborator-start-failed"));
    }
    if (m_batteryDomain.has || m_profileDomain.has || m_sessionDomain.has) {
        publishRestarting();
    }
}

void PowerServiceCoordinator::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_battery->stop();
    m_profiles->stop();
    m_session->stop();
    makePendingUncertain(QStringLiteral("service-stopped"));
}

void PowerServiceCoordinator::markDomainUnavailable(DomainState &state,
                                                    const quint64 generation,
                                                    const QString &reasonCode)
{
    state.generation = generation;
    state.phase = ServiceStartPhase::DomainUnavailable;
    state.reasonCode = sanitizeReasonCode(reasonCode);
}

void PowerServiceCoordinator::acceptBatteryFacts(const quint64 generation,
                                                 const BatteryFacts &facts)
{
    if (!generationCurrent(m_batteryDomain.state, generation)) {
        return;
    }
    m_batteryDomain.has = true;
    BatteryFacts sanitized;
    if (Assembly::sanitizeBatteryFacts(facts, m_snapshot.epoch, sanitized)) {
        m_batteryDomain.facts = std::move(sanitized);
        m_batteryDomain.valid = true;
        m_batteryDomain.state.phase = ServiceStartPhase::FactsObserved;
        m_batteryDomain.state.reasonCode.clear();
    } else {
        m_batteryDomain.valid = false;
        m_batteryDomain.state.phase = ServiceStartPhase::DomainDegraded;
        m_batteryDomain.state.reasonCode = QStringLiteral("battery-malformed");
    }
    enforceBatteryIdentityPrecedence();
    publishAssembled();
}

void PowerServiceCoordinator::enforceBatteryIdentityPrecedence()
{
    if (!m_profileDomain.has || !m_profileDomain.intrinsicValid) {
        return;
    }

    ProfileFacts revalidated;
    // AGENT-GUARD: Battery and keyboard identities own the shared Power1
    // handle namespace regardless of fact arrival order. Always project from
    // intrinsically sanitized profile truth: the publishable projection can
    // be invalid only because of a transient namespace collision and must not
    // become the source for later recovery.
    const QSet<QString> reservedOpaqueIds = m_batteryDomain.valid
        ? Assembly::batteryOpaqueIds(m_batteryDomain.facts)
        : QSet<QString>{};
    if (Assembly::sanitizeProfileFacts(
            m_profileDomain.intrinsicFacts, m_snapshot.epoch,
            reservedOpaqueIds, revalidated)) {
        m_profileDomain.facts = std::move(revalidated);
        m_profileDomain.valid = true;
        m_profileDomain.state.phase = ServiceStartPhase::FactsObserved;
        m_profileDomain.state.reasonCode.clear();
        return;
    }

    m_profileDomain.valid = false;
    m_profileDomain.state.phase = ServiceStartPhase::DomainDegraded;
    m_profileDomain.state.reasonCode = QStringLiteral("profile-malformed");
}

void PowerServiceCoordinator::acceptProfileFacts(const quint64 generation,
                                                 const ProfileFacts &facts)
{
    if (!generationCurrent(m_profileDomain.state, generation)) {
        return;
    }
    m_profileDomain.has = true;
    ProfileFacts intrinsic;
    // AGENT-GUARD: Preserve only intrinsically valid upstream truth. A later
    // battery mutation may lift collision suppression, but it must never
    // resurrect a profile fact set rejected for its own malformed content.
    if (Assembly::sanitizeProfileFacts(facts, m_snapshot.epoch, {}, intrinsic)) {
        m_profileDomain.intrinsicFacts = std::move(intrinsic);
        m_profileDomain.intrinsicValid = true;
        enforceBatteryIdentityPrecedence();
    } else {
        m_profileDomain.intrinsicValid = false;
        m_profileDomain.valid = false;
        m_profileDomain.state.phase = ServiceStartPhase::DomainDegraded;
        m_profileDomain.state.reasonCode = QStringLiteral("profile-malformed");
    }
    publishAssembled();
}

void PowerServiceCoordinator::acceptSessionFacts(const quint64 generation,
                                                 const SessionFacts &facts)
{
    if (!generationCurrent(m_sessionDomain.state, generation)) {
        return;
    }
    m_sessionDomain.has = true;
    SessionFacts sanitized;
    if (Assembly::sanitizeSessionFacts(facts, sanitized)) {
        m_sessionDomain.facts = std::move(sanitized);
        m_sessionDomain.valid = true;
        m_sessionDomain.state.phase = ServiceStartPhase::FactsObserved;
        m_sessionDomain.state.reasonCode.clear();
    } else {
        m_sessionDomain.valid = false;
        m_sessionDomain.state.phase = ServiceStartPhase::DomainDegraded;
        m_sessionDomain.state.reasonCode = QStringLiteral("session-malformed");
    }
    publishAssembled();
}

void PowerServiceCoordinator::acceptBatteryUnavailable(const quint64 generation,
                                                       const QString &reasonCode)
{
    if (!generationCurrent(m_batteryDomain.state, generation)) {
        return;
    }
    m_batteryDomain.has = true;
    m_batteryDomain.valid = false;
    markDomainUnavailable(m_batteryDomain.state, generation, reasonCode);
    enforceBatteryIdentityPrecedence();
    publishAssembled();
}

void PowerServiceCoordinator::acceptProfileUnavailable(const quint64 generation,
                                                       const QString &reasonCode)
{
    if (!generationCurrent(m_profileDomain.state, generation)) {
        return;
    }
    m_profileDomain.has = true;
    m_profileDomain.intrinsicValid = false;
    m_profileDomain.valid = false;
    markDomainUnavailable(m_profileDomain.state, generation, reasonCode);
    publishAssembled();
}

void PowerServiceCoordinator::acceptSessionUnavailable(const quint64 generation,
                                                       const QString &reasonCode)
{
    if (!generationCurrent(m_sessionDomain.state, generation)) {
        return;
    }
    m_sessionDomain.has = true;
    m_sessionDomain.valid = false;
    markDomainUnavailable(m_sessionDomain.state, generation, reasonCode);
    publishAssembled();
}

bool PowerServiceCoordinator::advanceEpoch()
{
    // AGENT-GUARD: Epochs strictly increase within this resident owner and
    // jump randomly so an owner replacement cannot collide with an earlier
    // epoch of this or another process. Exhausting the 64-bit space retains
    // the prior accepted snapshot rather than violating monotonicity.
    if (m_snapshot.epoch == std::numeric_limits<quint64>::max()) {
        return false;
    }
    const quint64 jump =
        1 + QRandomGenerator::system()->bounded(kMaximumEpochJump);
    if (m_snapshot.epoch > std::numeric_limits<quint64>::max() - jump) {
        m_snapshot.epoch = std::numeric_limits<quint64>::max();
        return true;
    }
    m_snapshot.epoch += jump;
    return true;
}

void PowerServiceCoordinator::advanceEpochAndPublish()
{
    if (!advanceEpoch()) {
        return;
    }
    makePendingUncertain(QStringLiteral("authority-replaced"));
    publishAssembled();
}

void PowerServiceCoordinator::acceptBatteryReplacement(const quint64 generation)
{
    if (!generationCurrent(m_batteryDomain.state, generation)) {
        return;
    }
    advanceEpochAndPublish();
}

void PowerServiceCoordinator::acceptProfileReplacement(const quint64 generation)
{
    if (!generationCurrent(m_profileDomain.state, generation)) {
        return;
    }
    advanceEpochAndPublish();
}

void PowerServiceCoordinator::acceptSessionReplacement(const quint64 generation)
{
    if (!generationCurrent(m_sessionDomain.state, generation)) {
        return;
    }
    advanceEpochAndPublish();
}

void PowerServiceCoordinator::publishRestarting()
{
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()
        || !advanceEpoch()) {
        return;
    }
    // AGENT-NOTE: Facts accepted under a stopped collaborator run are not
    // truth under the new run. The restart publication clears content and
    // capabilities, advances the epoch so pre-restart handles cannot survive,
    // and retains revision lineage, mirroring the accepted resident-audio
    // restart behavior.
    Snapshot restarting;
    restarting.protocolVersion = kProtocolVersion;
    restarting.epoch = m_snapshot.epoch;
    restarting.revision = m_snapshot.revision + 1;
    restarting.availability = Availability::Starting;
    restarting.reasonCode = QStringLiteral("collaborators-restarting");
    m_batteryDomain.has = false;
    m_batteryDomain.valid = false;
    m_profileDomain.has = false;
    m_profileDomain.valid = false;
    m_profileDomain.intrinsicValid = false;
    m_sessionDomain.has = false;
    m_sessionDomain.valid = false;
    m_snapshot = restarting;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void PowerServiceCoordinator::publishAssembled()
{
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    Assembly::AssemblyInput input;
    input.epoch = m_snapshot.epoch;
    input.revision = m_snapshot.revision + 1;
    input.batteryHas = m_batteryDomain.has;
    input.battery = m_batteryDomain.valid ? &m_batteryDomain.facts : nullptr;
    input.batteryUnavailable =
        m_batteryDomain.state.phase == ServiceStartPhase::DomainUnavailable;
    input.batteryReason = m_batteryDomain.state.reasonCode;
    input.profileHas = m_profileDomain.has;
    input.profiles = m_profileDomain.valid ? &m_profileDomain.facts : nullptr;
    input.profileUnavailable =
        m_profileDomain.state.phase == ServiceStartPhase::DomainUnavailable;
    input.profileReason = m_profileDomain.state.reasonCode;
    input.sessionHas = m_sessionDomain.has;
    input.session = m_sessionDomain.valid ? &m_sessionDomain.facts : nullptr;
    input.sessionUnavailable =
        m_sessionDomain.state.phase == ServiceStartPhase::DomainUnavailable;
    input.sessionReason = m_sessionDomain.state.reasonCode;

    Snapshot candidate = Assembly::assembleSnapshot(input);
    // AGENT-GUARD: The public snapshot is only ever a validateSnapshot-accepted
    // value. A whole-candidate failure despite per-domain validation falls
    // back to an empty validated Degraded snapshot and never publishes a
    // partially validated mixture.
    if (!validateSnapshot(candidate).accepted) {
        candidate = Snapshot{};
        candidate.protocolVersion = kProtocolVersion;
        candidate.epoch = input.epoch;
        candidate.revision = input.revision;
        candidate.availability = Availability::Degraded;
        candidate.reasonCode = QStringLiteral("snapshot-malformed");
        makePendingUncertain(QStringLiteral("snapshot-malformed"));
    }
    m_snapshot = candidate;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void PowerServiceCoordinator::makePendingUncertain(const QString &reasonCode)
{
    const auto pending = std::exchange(m_pending, {});
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        const PendingOperation &operation = it.value();
        // AGENT-GUARD: Completion is exactly-once. Emptying the pending table
        // before emission guarantees a late collaborator reply cannot produce
        // a second result for the same operation ID.
        Q_EMIT operationCompleted(
            it.key(),
            OperationResult{.kind = operation.kind,
                            .status = OperationStatus::Uncertain,
                            .initiatingEpoch = operation.epoch,
                            .initiatingRevision = operation.revision,
                            .observedEpoch = m_snapshot.epoch,
                            .observedRevision = m_snapshot.revision,
                            .reasonCode = reasonCode,
                            .diagnostic = {},
                            .wireValid = true});
    }
}

} // namespace QindaQt::Power
