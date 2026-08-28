// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace QindaQt::Power {

// Pending public operation limits are resident-orchestration policy, not wire
// limits; the accepted PB-0 protocol module stays frozen.
inline constexpr qsizetype kMaxServicePendingOperations = 8;

enum class ServiceStartPhase {
  Starting = 0,
  FactsObserved = 1,
  DomainDegraded = 2,
  DomainUnavailable = 3,
};

struct DomainState {
  quint64 generation = 0;
  ServiceStartPhase phase = ServiceStartPhase::Starting;
  QString reasonCode;
};

struct OperationSubmission {
  bool pending = false;
  quint64 operationId = 0;
  OperationResult immediateResult;
};

struct PowerServiceRequest {
  OperationKind kind = OperationKind::SetProfile;
  QString profileId;
  QString applicationName;
  QString reason;
  Handle handle;
  quint32 value = 0;
};

// AGENT-CONTRACT: Owns authoritative Power1 snapshot publication and operation
// lineage on one Qt thread. The three borrowed collaborators must share that
// thread and outlive this object; every upstream value is untrusted and is
// sanitized and validated before publication. Publication is atomic
// last-known-good: the coordinator only ever publishes a snapshot that passes
// validateSnapshot, a malformed domain loses only its own content and
// capability bits while every other accepted domain is retained, and a whole
// candidate that still fails validation degrades to an empty validated
// fallback. Epochs start nonzero-random and strictly increase on any upstream
// authority replacement, which restamps every public handle and makes
// dispatched operations Uncertain. Completion is exactly-once per submitted
// operation ID; stop() is a publication barrier making all pending operations
// Uncertain.
class PowerServiceCoordinator : public QObject {
  Q_OBJECT

public:
  PowerServiceCoordinator(BatteryCollaborator *battery, ProfileCollaborator *profiles,
                          SessionCollaborator *session, QObject *parent = nullptr);

  [[nodiscard]] const Snapshot &snapshot() const noexcept;
  [[nodiscard]] OperationSubmission submit(const PowerServiceRequest &request);
  void start();
  void stop();

Q_SIGNALS:
  void snapshotChanged(const QindaQt::Power::Snapshot &snapshot);
  void invalidated(quint64 epoch, quint64 revision);
  void operationCompleted(quint64 operationId,
                          const QindaQt::Power::OperationResult &result);

private Q_SLOTS:
  void acceptBatteryFacts(quint64 generation,
                          const QindaQt::Power::BatteryFacts &facts);
  void acceptProfileFacts(quint64 generation,
                          const QindaQt::Power::ProfileFacts &facts);
  void acceptSessionFacts(quint64 generation,
                          const QindaQt::Power::SessionFacts &facts);
  void acceptBatteryUnavailable(quint64 generation, const QString &reasonCode);
  void acceptProfileUnavailable(quint64 generation, const QString &reasonCode);
  void acceptSessionUnavailable(quint64 generation, const QString &reasonCode);
  void acceptBatteryReplacement(quint64 generation);
  void acceptProfileReplacement(quint64 generation);
  void acceptSessionReplacement(quint64 generation);
  void acceptBatteryOutcome(quint64 generation, quint64 operationId,
                            const QindaQt::Power::CollaboratorOutcome &outcome);
  void acceptProfileOutcome(quint64 generation, quint64 operationId,
                            const QindaQt::Power::CollaboratorOutcome &outcome);

private:
  struct PendingOperation {
    OperationKind kind = OperationKind::SetProfile;
    quint64 epoch = 0;
    quint64 revision = 0;
  };

  template <typename Facts>
  struct Domain {
    bool has = false;
    bool valid = false;
    Facts facts;
    DomainState state;
  };

  using BatteryDomain = Domain<BatteryFacts>;
  using ProfileDomain = Domain<ProfileFacts>;
  using SessionDomain = Domain<SessionFacts>;

  void connectBattery(BatteryCollaborator *battery);
  void connectProfile(ProfileCollaborator *profiles);
  void connectSession(SessionCollaborator *session);
  [[nodiscard]] bool generationCurrent(const DomainState &state,
                                       quint64 generation) const;
  [[nodiscard]] QString sanitizeReasonCode(const QString &reasonCode) const;
  void markDomainUnavailable(DomainState &state, quint64 generation,
                             const QString &reasonCode);
  void advanceEpochAndPublish();
  [[nodiscard]] bool advanceEpoch();
  void publishRestarting();
  void publishAssembled();
  void makePendingUncertain(const QString &reasonCode);
  [[nodiscard]] OperationResult immediateResult(const PowerServiceRequest &request,
                                                OperationStatus status,
                                                const QString &reasonCode) const;
  [[nodiscard]] QString validateRequest(const PowerServiceRequest &request) const;
  void deliverOutcome(quint64 operationId, const PendingOperation &pending,
                      const CollaboratorOutcome &outcome);

  BatteryCollaborator *m_battery = nullptr;
  ProfileCollaborator *m_profiles = nullptr;
  SessionCollaborator *m_session = nullptr;
  Snapshot m_snapshot;
  BatteryDomain m_batteryDomain;
  ProfileDomain m_profileDomain;
  SessionDomain m_sessionDomain;
  QHash<quint64, PendingOperation> m_pending;
  quint64 m_nextOperationId = 1;
  bool m_running = false;
};

} // namespace QindaQt::Power
