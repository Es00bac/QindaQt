// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/brightness_model/brightness_types.h>
#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Shell::PowerApplet {

// Presentation-only phase of the applet as a whole. It never claims service
// maturity: Loading covers a starting provider, Unavailable covers owner loss
// and fail-closed input, and Degraded keeps partial truth visible.
enum class ServicePhase : quint32 {
  Loading = 0,
  Ready = 1,
  Degraded = 2,
  Unavailable = 3,
};

// Coarse user-visible charge direction. It is a projection of Power1 truth,
// never an estimate: nothing here recomputes rates or time.
enum class ChargePhase : quint32 {
  Unknown = 0,
  Charging = 1,
  Discharging = 2,
  Full = 3,
  Empty = 4,
};

// Severity maps upstream warning and coarse-level truth one-to-one. No
// percentage thresholds exist in this module by design; thresholds are
// upstream policy (Power1), not applet presentation policy.
enum class ChargeSeverity : quint32 {
  Unknown = 0,
  Normal = 1,
  Full = 2,
  Low = 3,
  Critical = 4,
  Action = 5,
};

enum class RowAvailability : quint32 {
  Available = 0,
  Degraded = 1,
  Unavailable = 2,
};

// One aggregate-battery presentation row. Time remaining is published only
// through timeRemainingKnown, and `timeDirection` states which estimate it
// was (toward Empty or Full).
struct BatterySummaryRow {
  bool present = false;
  quint32 sourceCount = 0;
  bool percentageKnown = false;
  double percentage = 0.0;
  ChargePhase state = ChargePhase::Unknown;
  ChargeSeverity severity = ChargeSeverity::Unknown;
  bool netRateKnown = false;
  double netRateWatts = 0.0;
  bool timeRemainingKnown = false;
  ChargePhase timeDirection = ChargePhase::Unknown;
  qint64 timeRemainingSeconds = 0;
  QString accessibleName;
  QString accessibleDescription;

  friend bool operator==(const BatterySummaryRow &,
                         const BatterySummaryRow &) = default;
};

// One per-supply presentation row. `supplyId` is the Power1 opaque ID and
// `epoch` its owner generation; presentation never derives identity from
// hardware paths. `coarseLevel` is presented only when exact percentage truth
// is absent, mirroring the Power1 scalar contract.
struct BatteryRow {
  quint64 epoch = 0;
  QString supplyId;
  QString title;
  bool percentageKnown = false;
  double percentage = 0.0;
  bool coarseLevelKnown = false;
  Power::BatteryLevel coarseLevel = Power::BatteryLevel::Unknown;
  ChargePhase state = ChargePhase::Unknown;
  ChargeSeverity severity = ChargeSeverity::Unknown;
  bool timeRemainingKnown = false;
  ChargePhase timeDirection = ChargePhase::Unknown;
  qint64 timeRemainingSeconds = 0;
  RowAvailability availability = RowAvailability::Available;
  QString unavailableReason;
  QString accessibleName;
  QString accessibleDescription;

  friend bool operator==(const BatteryRow &, const BatteryRow &) = default;
};

enum class ControlLane : quint32 {
  Display = 0,
  Keyboard = 1,
};

// One brightness control row. `controlId` is the composition's stable display
// ID or the Power1 opaque keyboard ID. Current values appear only when their
// known flags hold, and `adjustable` repeats upstream capability truth.
struct BrightnessControlRow {
  ControlLane lane = ControlLane::Display;
  quint64 epoch = 0;
  QString controlId;
  QString name;
  RowAvailability availability = RowAvailability::Unavailable;
  QString unavailableReason;
  bool currentKnown = false;
  quint32 normalizedCurrent = 0;
  quint32 rawCurrent = 0;
  quint32 rawMaximum = 0;
  bool adjustable = false;
  QString accessibleName;
  QString accessibleDescription;

  friend bool operator==(const BrightnessControlRow &,
                         const BrightnessControlRow &) = default;
};

// Injected brightness composition view. Mirrors the Brightness::PowerView
// fence: when ownerAvailable is false the model is ignored entirely so a
// stale composition cannot survive its owner.
struct BrightnessView {
  bool ownerAvailable = false;
  Brightness::ModelSnapshot model;

  friend bool operator==(const BrightnessView &, const BrightnessView &) =
      default;
};

// The complete presentation projection. Rows are sorted by identity, so equal
// inputs always produce equal models regardless of upstream enumeration.
struct PowerAppletModel {
  ServicePhase phase = ServicePhase::Loading;
  QString diagnostic;
  BatterySummaryRow summary;
  QList<BatteryRow> supplies;
  QList<BrightnessControlRow> displayControls;
  QList<BrightnessControlRow> keyboardControls;

  friend bool operator==(const PowerAppletModel &, const PowerAppletModel &) =
      default;
};

} // namespace QindaQt::Shell::PowerApplet

Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::ServicePhase)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::ChargePhase)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::ChargeSeverity)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::RowAvailability)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::ControlLane)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::BatterySummaryRow)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::BatteryRow)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::BrightnessControlRow)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::BrightnessView)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::PowerAppletModel)
