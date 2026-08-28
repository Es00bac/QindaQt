// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QObject>

namespace QindaQt::Power {

enum class CollaboratorStatus : quint32 {
  Succeeded = 0,
  Unsupported = 1,
  Failed = 2,
  Uncertain = 3,
};

// An untrusted upstream completion value. The coordinator accepts only known
// status values and stable bounded reason tokens; one invalid field replaces
// the whole classification with a protocol-valid `upstream-malformed` failure.
struct CollaboratorOutcome {
  CollaboratorStatus status = CollaboratorStatus::Failed;
  QString reasonCode;
  QString diagnostic;
};

// Facts owned by the battery authority (UPower): supply inventory, keyboard
// backlights, and AC/on-battery truth. Handle epochs in facts are ignored; the
// coordinator stamps the resident epoch on every public handle using the
// opaque ID as the stable device key.
struct BatteryFacts {
  bool acPresent = false;
  bool onBattery = false;
  QList<PowerSupply> supplies;
  QList<KeyboardBacklight> keyboardBacklights;
};

// Facts owned by the power-profile authority: supported profiles, the active
// profile, and epoch-scoped holds. Hold handle epochs are ignored and restamped
// by the coordinator.
struct ProfileFacts {
  ProfileState profiles;
};

// Facts owned by the session authority (logind): lid/dock/sleep truth and the
// sanitized inhibitor summary. No field can represent UID or PID.
struct SessionFacts {
  bool lidPresent = false;
  bool lidClosed = false;
  bool docked = false;
  bool preparingForSleep = false;
  QList<Inhibitor> inhibitors;
};

// AGENT-CONTRACT: Each interface below is one upstream authority seam. An
// implementation receives calls on the coordinator's Qt thread, returns a
// fresh nonzero generation from start() before that run can publish, stamps
// every emission with the producing generation (an equality token, never an
// ordered value), and publishes only immutable value copies through signals.
// stop() invalidates that run; emissions from a stopped or superseded
// generation are dropped by the coordinator. Real UPower, power-profiles-daemon,
// and logind adapters arrive in later slices and must not leak thread-affine
// or raw upstream identity (object paths, serial numbers, UID, PID) through
// these boundaries. See docs/wiki/architecture/power-service.md.
class BatteryCollaborator : public QObject {
  Q_OBJECT

public:
  explicit BatteryCollaborator(QObject *parent = nullptr)
      : QObject(parent)
  {
  }
  ~BatteryCollaborator() override = default;

  [[nodiscard]] virtual quint64 start() = 0;
  virtual void stop() = 0;
  virtual void submitSetKeyboardBrightness(quint64 operationId, const Handle &device,
                                           quint32 value) = 0;

Q_SIGNALS:
  void factsChanged(quint64 generation,
                    const QindaQt::Power::BatteryFacts &facts);
  void statusUnavailable(quint64 generation, const QString &reasonCode);
  void authorityReplaced(quint64 generation);
  void operationFinished(quint64 generation, quint64 operationId,
                         const QindaQt::Power::CollaboratorOutcome &outcome);
};

class ProfileCollaborator : public QObject {
  Q_OBJECT

public:
  explicit ProfileCollaborator(QObject *parent = nullptr)
      : QObject(parent)
  {
  }
  ~ProfileCollaborator() override = default;

  [[nodiscard]] virtual quint64 start() = 0;
  virtual void stop() = 0;
  virtual void submitSetProfile(quint64 operationId, const QString &profileId) = 0;
  virtual void submitAcquireProfileHold(quint64 operationId, const QString &profileId,
                                        const QString &applicationName,
                                        const QString &reason) = 0;
  virtual void submitReleaseProfileHold(quint64 operationId, const Handle &hold) = 0;

Q_SIGNALS:
  void factsChanged(quint64 generation,
                    const QindaQt::Power::ProfileFacts &facts);
  void statusUnavailable(quint64 generation, const QString &reasonCode);
  void authorityReplaced(quint64 generation);
  void operationFinished(quint64 generation, quint64 operationId,
                         const QindaQt::Power::CollaboratorOutcome &outcome);
};

class SessionCollaborator : public QObject {
  Q_OBJECT

public:
  explicit SessionCollaborator(QObject *parent = nullptr)
      : QObject(parent)
  {
  }
  ~SessionCollaborator() override = default;

  [[nodiscard]] virtual quint64 start() = 0;
  virtual void stop() = 0;

Q_SIGNALS:
  void factsChanged(quint64 generation,
                    const QindaQt::Power::SessionFacts &facts);
  void statusUnavailable(quint64 generation, const QString &reasonCode);
  void authorityReplaced(quint64 generation);
};

} // namespace QindaQt::Power

Q_DECLARE_METATYPE(QindaQt::Power::CollaboratorOutcome)
Q_DECLARE_METATYPE(QindaQt::Power::BatteryFacts)
Q_DECLARE_METATYPE(QindaQt::Power::ProfileFacts)
Q_DECLARE_METATYPE(QindaQt::Power::SessionFacts)
