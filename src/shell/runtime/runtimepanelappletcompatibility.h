// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QVector>

namespace QindaQt::Shell {

// Pure compatibility boundary for profile identifiers that predate compiled
// first-party applets. It preserves instance identity/settings and never
// invents applets that were not selected by the profile.
class RuntimePanelAppletCompatibility final {
public:
    RuntimePanelAppletCompatibility() = delete;

    [[nodiscard]] static QVector<Profiles::AppletSpec> normalize(
        const QVector<Profiles::AppletSpec> &applets);
};

} // namespace QindaQt::Shell
