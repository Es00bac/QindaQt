// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Power {

enum class InternalBrightnessAdmission : quint32 {
  Admitted = 0,
  UnknownDevice = 1,
  NotSelected = 2,
  Ambiguous = 3,
  DeviceUnavailable = 4,
};

// AGENT-CONTRACT: The one internal-panel brightness target rule (ADR-0148),
// shared by Power1 request validation, the production sysfs apply step,
// PowerClient preflight, and Settings admission. Call it; never restate it,
// or a control could be offered that the service refuses. Pure and reentrant.
//
// Candidates are devices with a usable maximum. The kernel type preference
// firmware > platform > raw selects one tier among them: every candidate in
// that tier is Ambiguous when there is more than one, lower tiers are
// NotSelected, and the selected device is Admitted only while it is Ok with an
// observed value. A read-only or unreadable selected device never falls back
// to a lower-preference device. Power1 cannot observe connector topology, so
// this rule does not claim the later KWin provider's single-connector check.
[[nodiscard]] InternalBrightnessAdmission
internalBrightnessAdmission(const QList<InternalBacklight> &devices,
                            const QString &opaqueId);

} // namespace QindaQt::Power
