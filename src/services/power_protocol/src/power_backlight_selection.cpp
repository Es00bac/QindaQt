// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_backlight_selection.h>

namespace QindaQt::Power {
namespace {

// Lower is preferred. An out-of-vocabulary kind is never a candidate.
int preference(const BacklightKind kind) {
  switch (kind) {
  case BacklightKind::Firmware:
    return 0;
  case BacklightKind::Platform:
    return 1;
  case BacklightKind::Raw:
    return 2;
  }
  return -1;
}

bool candidate(const InternalBacklight &device) {
  return device.maximum > 0 && preference(device.kind) >= 0;
}

} // namespace

InternalBrightnessAdmission
internalBrightnessAdmission(const QList<InternalBacklight> &devices,
                            const QString &opaqueId) {
  const InternalBacklight *target = nullptr;
  int selected = -1;
  for (const InternalBacklight &device : devices) {
    if (target == nullptr && !opaqueId.isEmpty() &&
        device.handle.opaqueId == opaqueId) {
      target = &device;
    }
    if (candidate(device) &&
        (selected < 0 || preference(device.kind) < selected)) {
      selected = preference(device.kind);
    }
  }
  if (target == nullptr) {
    return InternalBrightnessAdmission::UnknownDevice;
  }
  if (!candidate(*target)) {
    return InternalBrightnessAdmission::DeviceUnavailable;
  }
  if (preference(target->kind) != selected) {
    return InternalBrightnessAdmission::NotSelected;
  }
  qsizetype peers = 0;
  for (const InternalBacklight &device : devices) {
    if (candidate(device) && preference(device.kind) == selected) {
      ++peers;
    }
  }
  if (peers > 1) {
    return InternalBrightnessAdmission::Ambiguous;
  }
  if (target->status != BacklightStatus::Ok || !target->observedKnown ||
      target->observed > target->maximum) {
    return InternalBrightnessAdmission::DeviceUnavailable;
  }
  return InternalBrightnessAdmission::Admitted;
}

} // namespace QindaQt::Power
