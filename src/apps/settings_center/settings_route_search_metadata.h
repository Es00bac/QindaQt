// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings_route.h"

namespace QindaQt::Apps::SettingsCenter {

// AGENT-CONTRACT: Fills the search keywords and deep-link destinations of one
// built-in route descriptor (ADR-0257). The data lives beside the registry,
// not in the route pages, because the shell must never reach into a page's
// QML to learn what it contains. The switch over the closed component enum
// is exhaustive on purpose: a new route fails -Wswitch until it is given
// search terms. Only identity, keywords and destinations are touched; title,
// availability and order stay the registry's.
void applyBuiltInSearchMetadata(SettingsRoute &route);

} // namespace QindaQt::Apps::SettingsCenter
