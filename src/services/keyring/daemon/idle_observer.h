// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/platform/idle_observation/idle_observation.h>
namespace qindaqt::keyring::service {
// Private consumer names only; the public platform module owns protocol/FD
// lifetimes. Keep keyring persistence and collection policy out of that module.
using IdleObservation = QindaQt::Platform::Idle::IdleObservation;
using WaylandIdleObservation = QindaQt::Platform::Idle::WaylandIdleObservation;
}
