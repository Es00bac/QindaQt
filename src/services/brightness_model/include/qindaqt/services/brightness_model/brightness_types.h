// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Brightness {

enum class ControlAvailability : quint32 {
  Available = 0,
  Degraded = 1,
  Unavailable = 2,
};

enum class ControlReason : quint32 {
  None = 0,
  PowerOwnerUnavailable = 1,
  PowerServiceUnavailable = 2,
  CapabilityUnavailable = 3,
  DeviceNotMapped = 4,
  DeviceMissing = 5,
  ObservationUnavailable = 6,
  ProviderDegraded = 7,
  ProviderUnavailable = 8,
  LineageMismatch = 9,
};

// AGENT-CONTRACT: This is a brightness-lane fixture, not a Display1 output. The
// mapping is an epoch-scoped opaque Power1 handle supplied by later
// composition; connector names and topology mutation fields are intentionally
// not representable.
struct DisplayFixture {
  QString stableId;
  QString replicationSourceStableId;
  bool ambiguousIdentity = false;
  Power::Handle powerBacklightHandle;

  friend bool operator==(const DisplayFixture &,
                         const DisplayFixture &) = default;
};

struct FixtureSnapshot {
  bool ownerAvailable = false;
  QString serviceEpoch;
  quint64 revision = 0;
  QList<DisplayFixture> displays;

  friend bool operator==(const FixtureSnapshot &,
                         const FixtureSnapshot &) = default;
};

struct PowerView {
  // AGENT-GUARD: ownerAvailable is the freshness fence. When false, composition
  // ignores snapshot entirely so a client cache cannot survive owner loss.
  bool ownerAvailable = false;
  Power::Snapshot snapshot;

  friend bool operator==(const PowerView &, const PowerView &) = default;
};

struct DisplayControl {
  QString stableId;
  QList<QString> memberStableIds;
  bool persistenceAllowed = true;
  ControlAvailability availability = ControlAvailability::Unavailable;
  ControlReason reason = ControlReason::DeviceNotMapped;
  Power::BacklightReason providerReason = Power::BacklightReason::None;
  Power::Handle providerHandle;
  bool currentKnown = false;
  quint32 rawMinimum = 0;
  quint32 rawCurrent = 0;
  quint32 rawMaximum = 0;
  quint32 normalizedCurrent = 0;

  friend bool operator==(const DisplayControl &,
                         const DisplayControl &) = default;
};

struct KeyboardControl {
  Power::Handle handle;
  QString name;
  ControlAvailability availability = ControlAvailability::Unavailable;
  ControlReason reason = ControlReason::ObservationUnavailable;
  bool currentKnown = false;
  quint32 rawMinimum = 0;
  quint32 rawCurrent = 0;
  quint32 rawMaximum = 0;
  quint32 normalizedCurrent = 0;
  bool canSet = false;

  friend bool operator==(const KeyboardControl &,
                         const KeyboardControl &) = default;
};

struct ModelSnapshot {
  QString fixtureEpoch;
  quint64 fixtureRevision = 0;
  bool powerOwnerAvailable = false;
  quint64 powerEpoch = 0;
  quint64 powerRevision = 0;
  QList<DisplayControl> displays;
  QList<KeyboardControl> keyboards;

  friend bool operator==(const ModelSnapshot &,
                         const ModelSnapshot &) = default;
};

} // namespace QindaQt::Brightness

Q_DECLARE_METATYPE(QindaQt::Brightness::ControlAvailability)
Q_DECLARE_METATYPE(QindaQt::Brightness::ControlReason)
Q_DECLARE_METATYPE(QindaQt::Brightness::DisplayFixture)
Q_DECLARE_METATYPE(QindaQt::Brightness::FixtureSnapshot)
Q_DECLARE_METATYPE(QindaQt::Brightness::PowerView)
Q_DECLARE_METATYPE(QindaQt::Brightness::DisplayControl)
Q_DECLARE_METATYPE(QindaQt::Brightness::KeyboardControl)
Q_DECLARE_METATYPE(QindaQt::Brightness::ModelSnapshot)
