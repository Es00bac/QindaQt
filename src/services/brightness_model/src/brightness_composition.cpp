// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/brightness_model/brightness_composition.h>

#include <qindaqt/services/brightness_model/brightness_math.h>
#include <qindaqt/services/brightness_model/brightness_validation.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtCore/QHash>

#include <algorithm>

namespace QindaQt::Brightness {
namespace {

CompositionResult failed(const CompositionError error, const char *reasonCode) {
  return {.snapshot = {},
          .error = error,
          .reasonCode = QString::fromLatin1(reasonCode)};
}

const DisplayFixture *
rootFixture(const DisplayFixture &display,
            const QHash<QString, const DisplayFixture *> &fixtures) {
  // AGENT-GUARD: composeBrightness validates the complete fixture before this
  // traversal. Calling this helper on unchecked values would make a missing
  // root dereferenceable on the following loop iteration.
  const DisplayFixture *cursor = &display;
  while (!cursor->replicationSourceStableId.isEmpty()) {
    cursor = fixtures.value(cursor->replicationSourceStableId, nullptr);
  }
  return cursor;
}

bool powerStateUsable(const Power::Snapshot &snapshot) {
  return snapshot.availability == Power::Availability::Ready ||
         snapshot.availability == Power::Availability::Degraded;
}

bool applyCurrentTruth(const quint32 current, const quint32 maximum,
                       bool &known, quint32 &rawCurrent, quint32 &rawMaximum,
                       quint32 &normalized) {
  const NormalizedResult result = normalizeRaw(0, maximum, current);
  if (!result.succeeded()) {
    return false;
  }
  known = true;
  rawCurrent = current;
  rawMaximum = maximum;
  normalized = result.value;
  return true;
}

} // namespace

CompositionResult composeBrightness(const FixtureSnapshot &fixture,
                                    const PowerView &power) {
  if (!validateFixture(fixture).accepted()) {
    return failed(CompositionError::InvalidFixture,
                  "invalid-brightness-fixture");
  }
  if (power.ownerAvailable &&
      !Power::validateSnapshot(power.snapshot).accepted) {
    return failed(CompositionError::InvalidPowerSnapshot,
                  "invalid-power-snapshot");
  }

  ModelSnapshot model;
  model.fixtureEpoch = fixture.serviceEpoch;
  model.fixtureRevision = fixture.revision;
  model.powerOwnerAvailable = power.ownerAvailable;
  if (power.ownerAvailable) {
    model.powerEpoch = power.snapshot.epoch;
    model.powerRevision = power.snapshot.revision;
  }

  QHash<QString, const DisplayFixture *> fixtures;
  QList<const DisplayFixture *> roots;
  for (const DisplayFixture &display : fixture.displays) {
    fixtures.insert(display.stableId, &display);
    if (display.replicationSourceStableId.isEmpty()) {
      roots.push_back(&display);
    }
  }
  std::ranges::sort(
      roots, [](const DisplayFixture *left, const DisplayFixture *right) {
        return left->stableId < right->stableId;
      });

  QHash<QString, const Power::InternalBacklight *> internalBacklights;
  if (power.ownerAvailable) {
    for (const Power::InternalBacklight &backlight :
         power.snapshot.internalBacklights) {
      internalBacklights.insert(backlight.handle.opaqueId, &backlight);
    }
  }

  const bool usablePower =
      power.ownerAvailable && powerStateUsable(power.snapshot);
  const bool internalCapability =
      usablePower && power.snapshot.capabilities.testFlag(
                         Power::Capability::InternalBacklight);

  for (const DisplayFixture *root : roots) {
    DisplayControl control;
    control.stableId = root->stableId;
    for (const DisplayFixture &member : fixture.displays) {
      if (rootFixture(member, fixtures) == root) {
        control.memberStableIds.push_back(member.stableId);
        if (member.ambiguousIdentity) {
          control.persistenceAllowed = false;
        }
      }
    }
    std::ranges::sort(control.memberStableIds);

    if (!power.ownerAvailable) {
      control.reason = ControlReason::PowerOwnerUnavailable;
    } else if (!usablePower) {
      control.reason = ControlReason::PowerServiceUnavailable;
    } else if (!internalCapability) {
      control.reason = ControlReason::CapabilityUnavailable;
    } else if (!root->powerBacklightHandle.isValid()) {
      control.reason = ControlReason::DeviceNotMapped;
    } else if (root->powerBacklightHandle.epoch != power.snapshot.epoch) {
      control.reason = ControlReason::LineageMismatch;
    } else {
      const Power::InternalBacklight *backlight = internalBacklights.value(
          root->powerBacklightHandle.opaqueId, nullptr);
      if (backlight == nullptr) {
        control.reason = ControlReason::DeviceMissing;
      } else {
        control.providerHandle = backlight->handle;
        control.providerReason = backlight->reason;
        if (backlight->observedKnown &&
            !applyCurrentTruth(backlight->observed, backlight->maximum,
                               control.currentKnown, control.rawCurrent,
                               control.rawMaximum, control.normalizedCurrent)) {
          return failed(CompositionError::InvalidBrightnessValue,
                        "invalid-display-brightness-value");
        }
        switch (backlight->status) {
        case Power::BacklightStatus::Ok:
          if (control.currentKnown) {
            control.availability = ControlAvailability::Available;
            control.reason = ControlReason::None;
          } else {
            control.reason = ControlReason::ObservationUnavailable;
          }
          break;
        case Power::BacklightStatus::Degraded:
          control.availability = ControlAvailability::Degraded;
          control.reason = ControlReason::ProviderDegraded;
          break;
        case Power::BacklightStatus::Unavailable:
          control.reason = ControlReason::ProviderUnavailable;
          break;
        }
      }
    }
    model.displays.push_back(std::move(control));
  }

  const bool keyboardCapability =
      usablePower && power.snapshot.capabilities.testFlag(
                         Power::Capability::KeyboardBacklight);
  if (keyboardCapability) {
    QList<const Power::KeyboardBacklight *> keyboards;
    keyboards.reserve(power.snapshot.keyboardBacklights.size());
    for (const Power::KeyboardBacklight &keyboard :
         power.snapshot.keyboardBacklights) {
      keyboards.push_back(&keyboard);
    }
    std::ranges::sort(keyboards, [](const Power::KeyboardBacklight *left,
                                    const Power::KeyboardBacklight *right) {
      return left->handle.opaqueId < right->handle.opaqueId;
    });
    for (const Power::KeyboardBacklight *keyboard : keyboards) {
      KeyboardControl control;
      control.handle = keyboard->handle;
      control.name = keyboard->name;
      if (keyboard->valueKnown) {
        if (!applyCurrentTruth(keyboard->value, keyboard->maximum,
                               control.currentKnown, control.rawCurrent,
                               control.rawMaximum, control.normalizedCurrent)) {
          return failed(CompositionError::InvalidBrightnessValue,
                        "invalid-keyboard-brightness-value");
        }
        control.availability = ControlAvailability::Available;
        control.reason = ControlReason::None;
        control.canSet = keyboard->canSet;
      }
      model.keyboards.push_back(std::move(control));
    }
  }

  return {.snapshot = std::move(model),
          .error = CompositionError::None,
          .reasonCode = {}};
}

} // namespace QindaQt::Brightness
