// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_types.h>
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_types.h"

namespace QindaQt::StatusNotifierApplet {

// AGENT-CONTRACT: Pure functional presentation model projecting the S1
// StatusNotifier TrayPresentation plus the registry's item descriptors into
// immutable, bounded UI structures for QML presentation. This class owns no
// state, handles, or threads; it executes deterministic projections only
// (same inputs, same output; row order follows the S1 presentation's stable
// order). Projections fail closed: a denied read grant withholds every row,
// and a presentation item without a matching descriptor projects with
// descriptor facts withheld rather than guessing.
class StatusNotifierAppletModel final {
public:
    StatusNotifierAppletModel() = delete;

    // Projects one S1 tray presentation into the applet surface. Descriptors
    // are matched to presentation items by item identity (unique across live
    // owners by registry contract); a missing match yields title=identity and
    // no menu facts — fail-closed, never a crash. `readGranted == false`
    // withholds all observation: Unavailable phase, registered reason code,
    // and no rows regardless of presentation content.
    [[nodiscard]] static StatusNotifierAppletProjection project(
        const QindaQt::StatusNotifier::TrayPresentation &presentation,
        const QList<QindaQt::StatusNotifier::ItemDescriptor> &descriptors,
        bool readGranted,
        const StatusNotifierAppletTexts &texts = {});

    // Flattens one S1 menu payload into bounded display rows. Defense in
    // depth: the S1 admission gate already validated the payload, but this
    // projection re-enforces the depth bound itself — entries deeper than
    // `maxDepth`, invisible entries, entries with an invalid/dropped parent
    // chain, and their descendants are dropped, never presented, never crash.
    [[nodiscard]] static QList<StatusNotifierMenuRow> projectMenu(
        const QindaQt::StatusNotifier::MenuPayload &menu,
        int maxDepth = 4);
};

} // namespace QindaQt::StatusNotifierApplet
