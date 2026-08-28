// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtCore/QSet>
#include <QtCore/QString>

namespace QindaQt::Power {
namespace Assembly {

struct AssemblyInput {
  quint64 epoch = 0;
  quint64 revision = 0;
  bool batteryHas = false;
  const BatteryFacts *battery = nullptr;
  bool batteryUnavailable = false;
  QString batteryReason;
  bool profileHas = false;
  const ProfileFacts *profiles = nullptr;
  bool profileUnavailable = false;
  QString profileReason;
  bool sessionHas = false;
  const SessionFacts *session = nullptr;
  bool sessionUnavailable = false;
  QString sessionReason;
};

// Sanitizers copy one domain of untrusted upstream facts into protocol-valid
// output stamped with the resident epoch. They return false when the domain is
// malformed beyond sanitization (bounds, numbers, enums, lineage); the caller
// then degrades only that domain. Cross-domain opaque-ID uniqueness is checked
// by the profile sanitizer against the battery IDs because handles share one
// Power1 namespace.
[[nodiscard]] bool sanitizeBatteryFacts(const BatteryFacts &input, quint64 epoch,
                                        BatteryFacts &output);
[[nodiscard]] bool sanitizeProfileFacts(const ProfileFacts &input, quint64 epoch,
                                        const QSet<QString> &reservedOpaqueIds,
                                        ProfileFacts &output);
[[nodiscard]] bool sanitizeSessionFacts(const SessionFacts &input,
                                        SessionFacts &output);
[[nodiscard]] QSet<QString> batteryOpaqueIds(const BatteryFacts &facts);
[[nodiscard]] Snapshot assembleSnapshot(const AssemblyInput &input);

} // namespace Assembly
} // namespace QindaQt::Power
