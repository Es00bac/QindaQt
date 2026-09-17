// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/smart_lights_store/smart_lights_configuration.h>
#include <qindaqt/shell/smart_lights_applet/smart_lights_applet_types.h>

#include <QtCore/QHash>

namespace QindaQt::Shell::SmartLightsApplet
{

// Projects one bounded, owned value model for the panel.
//
// AGENT-CONTRACT: this function is pure. It is the single place that decides
// what the user is shown and what they are allowed to touch, so a control that
// is enabled here must be dispatchable, and one that is not must be inert.
// `rowIds` maps device MAC to the session-scoped opaque token QML sees; a
// device missing from it is not projected, because presenting a row the
// controller cannot resolve back to a device would offer a dead control.
[[nodiscard]] SmartLightsAppletModel projectSmartLightsApplet(
    const Wiz::Snapshot &snapshot,
    const QList<SmartLights::StoredPreset> &presets,
    const QHash<QString, QString> &rowIds,
    bool readGranted,
    bool controlGranted);

} // namespace QindaQt::Shell::SmartLightsApplet
