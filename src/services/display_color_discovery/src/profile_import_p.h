// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_color_discovery/profile_discovery.h>

namespace QindaQt::DisplayColor
{

// Validates the source, computes the lineage fingerprint, and atomically
// copies the profile into the first injected UserImported root. Split from
// profile_discovery.cpp so enumeration and import stay independently
// reviewable; ProfileDiscovery::importUserProfile delegates here.
ImportResult importProfileFromSource(const QList<DiscoveryRoot> &roots,
                                     const DiscoveryLimits &limits,
                                     const QString &sourcePath);

} // namespace QindaQt::DisplayColor
