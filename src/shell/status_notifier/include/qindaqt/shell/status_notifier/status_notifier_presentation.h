// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_registry.h>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>

namespace QindaQt::StatusNotifier
{

struct PresentationInput {
    // True while the StatusNotifierWatcher is reachable through the injected
    // transport. A watcher loss degrades presentation even with items left.
    bool transportLive = false;

    friend bool operator==(const PresentationInput &, const PresentationInput &) = default;
};

// AGENT-CONTRACT: Pure projection from registry state to tray presentation.
// Same registry state and input always yield an equal TrayPresentation with a
// stable item order, so QML and assistive technology observe no phantom
// reorder. No I/O, clock, or transport access happens here.
[[nodiscard]] TrayPresentation projectPresentation(const StatusNotifierRegistry &registry,
                                                   const PresentationInput &input);

} // namespace QindaQt::StatusNotifier
