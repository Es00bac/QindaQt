// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_control_rows_p.h"

#include <algorithm>
#include <tuple>

namespace QindaQt::Shell::PowerApplet::detail {
namespace {

using Brightness::ControlAvailability;
using Brightness::ControlReason;

QString controlUnavailablePhrase(const ControlReason reason) {
  if (!inVocabulary(reason, 9U)) {
    return QStringLiteral("Brightness control is unavailable");
  }
  switch (reason) {
  case ControlReason::PowerOwnerUnavailable:
    return QStringLiteral("Power service owner is unavailable");
  case ControlReason::PowerServiceUnavailable:
    return QStringLiteral("Power service is unavailable");
  case ControlReason::CapabilityUnavailable:
    return QStringLiteral("Brightness capability is unavailable");
  case ControlReason::DeviceNotMapped:
    return QStringLiteral("Brightness device is not mapped");
  case ControlReason::DeviceMissing:
    return QStringLiteral("Brightness device is missing");
  case ControlReason::ObservationUnavailable:
    return QStringLiteral("Brightness value cannot be read");
  case ControlReason::ProviderDegraded:
    return QStringLiteral("Brightness provider is degraded");
  case ControlReason::ProviderUnavailable:
    return QStringLiteral("Brightness provider is unavailable");
  case ControlReason::LineageMismatch:
    return QStringLiteral("Brightness state is stale");
  case ControlReason::None:
    break;
  }
  return QStringLiteral("Brightness control is unavailable");
}

RowAvailability rowAvailabilityOf(const ControlAvailability value) {
  if (!inVocabulary(value, 2U)) {
    return RowAvailability::Unavailable;
  }
  switch (value) {
  case ControlAvailability::Available:
    return RowAvailability::Available;
  case ControlAvailability::Degraded:
    return RowAvailability::Degraded;
  case ControlAvailability::Unavailable:
    break;
  }
  return RowAvailability::Unavailable;
}

BrightnessControlRow projectComposedDisplay(
    const Brightness::DisplayControl &control) {
  BrightnessControlRow row;
  row.lane = ControlLane::Display;
  row.controlId = control.stableId;
  row.name = control.stableId;
  row.availability = rowAvailabilityOf(control.availability);
  if (row.availability != RowAvailability::Available) {
    row.unavailableReason = controlUnavailablePhrase(control.reason);
  }
  // AGENT-NOTE: Display brightness is provider-adjusted through KWin in
  // later slices; Power1 v1 defines no display-set operation, so the row is
  // never marked adjustable here regardless of availability.
  row.adjustable = false;
  row.currentKnown = control.currentKnown;
  row.normalizedCurrent = control.normalizedCurrent;
  row.rawCurrent = control.rawCurrent;
  row.rawMaximum = control.rawMaximum;
  row.accessibleName = QStringLiteral("Screen brightness");
  row.accessibleDescription =
      row.availability == RowAvailability::Available
          ? QStringLiteral("Screen brightness control.")
          : QStringLiteral("Screen brightness control. %1.")
                .arg(row.unavailableReason);
  return row;
}

BrightnessControlRow projectComposedKeyboard(
    const Brightness::KeyboardControl &control) {
  BrightnessControlRow row;
  row.lane = ControlLane::Keyboard;
  row.epoch = control.handle.epoch;
  row.controlId = control.handle.opaqueId;
  row.name = control.name;
  row.availability = rowAvailabilityOf(control.availability);
  if (row.availability != RowAvailability::Available) {
    row.unavailableReason = controlUnavailablePhrase(control.reason);
  }
  row.currentKnown = control.currentKnown;
  row.normalizedCurrent = control.normalizedCurrent;
  row.rawCurrent = control.rawCurrent;
  row.rawMaximum = control.rawMaximum;
  row.adjustable =
      control.canSet && row.availability == RowAvailability::Available;
  row.accessibleName =
      QStringLiteral("Keyboard backlight%1")
          .arg(row.currentKnown
                   ? QStringLiteral(": %1%")
                         .arg(control.normalizedCurrent / 100)
                   : QString());
  row.accessibleDescription =
      QStringLiteral("Keyboard backlight control, %1.")
          .arg(row.adjustable ? QStringLiteral("adjustable")
                              : QStringLiteral("not adjustable"));
  return row;
}

BrightnessControlRow fallbackRow(const ControlLane lane, const quint64 epoch,
                                 const QString &controlId, const QString &name,
                                 const QString &accessibleName) {
  BrightnessControlRow row;
  row.lane = lane;
  row.epoch = epoch;
  row.controlId = controlId;
  row.name = name;
  row.availability = RowAvailability::Unavailable;
  row.unavailableReason =
      QStringLiteral("Brightness composition is unavailable");
  row.currentKnown = false;
  row.adjustable = false;
  row.accessibleName = accessibleName;
  row.accessibleDescription =
      QStringLiteral("%1 control. %2.").arg(accessibleName,
                                            row.unavailableReason);
  return row;
}

void sortByIdentity(QList<BrightnessControlRow> &rows) {
  // AGENT-GUARD: Sorting must be total (ID and epoch) so a hostile generation
  // carrying duplicate identities still projects deterministically.
  std::ranges::sort(rows, [](const BrightnessControlRow &left,
                             const BrightnessControlRow &right) {
    return std::tie(left.controlId, left.epoch) <
           std::tie(right.controlId, right.epoch);
  });
}

} // namespace

QString appendPhrase(const QString &base, const QString &phrase) {
  return phrase.isEmpty() ? base : QStringLiteral("%1, %2").arg(base, phrase);
}

void projectControls(const Power::Snapshot &snapshot,
                     const BrightnessView &brightness,
                     PowerAppletModel &model) {
  const bool displayCapable =
      snapshot.capabilities.testFlag(Power::Capability::InternalBacklight);
  const bool keyboardCapable =
      snapshot.capabilities.testFlag(Power::Capability::KeyboardBacklight);

  if (brightness.ownerAvailable) {
    for (const Brightness::DisplayControl &control :
         brightness.model.displays) {
      model.displayControls.append(projectComposedDisplay(control));
    }
    for (const Brightness::KeyboardControl &control :
         brightness.model.keyboards) {
      model.keyboardControls.append(projectComposedKeyboard(control));
    }
    sortByIdentity(model.displayControls);
    sortByIdentity(model.keyboardControls);
    return;
  }
  // AGENT-GUARD: Without its composition owner the applet may keep device
  // identity visible but must present every control unavailable and
  // non-adjustable. Publishing snapshot raw values here would let a stale
  // generation look adjustable.
  if (displayCapable) {
    for (const Power::InternalBacklight &device :
         snapshot.internalBacklights) {
      model.displayControls.append(
          fallbackRow(ControlLane::Display, device.handle.epoch,
                      device.handle.opaqueId, device.deviceName,
                      QStringLiteral("Screen brightness")));
    }
  }
  if (keyboardCapable) {
    for (const Power::KeyboardBacklight &device :
         snapshot.keyboardBacklights) {
      model.keyboardControls.append(
          fallbackRow(ControlLane::Keyboard, device.handle.epoch,
                      device.handle.opaqueId, device.name,
                      QStringLiteral("Keyboard backlight")));
    }
  }
  sortByIdentity(model.displayControls);
  sortByIdentity(model.keyboardControls);
}

} // namespace QindaQt::Shell::PowerApplet::detail
