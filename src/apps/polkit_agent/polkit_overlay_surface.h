// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QQuickWindow;

namespace QindaQt::Apps::PolkitAgent {

// Presents an already-constructed QQuickWindow as a full-output, keyboard-
// exclusive layer-shell overlay: the modal shape every authentication
// dialog needs (no other window may steal the keystrokes meant for a
// password).
//
// AGENT-NOTE: this duplicates the small technique
// qindaqt/shell_surface/layer_shell_notification_surface.cpp uses (raw
// LayerShellQt::Window calls on an unshown QQuickWindow) rather than
// depending on that module. shell_surface's own public surfaces are shaped
// for panels (PanelSurfaceBackend) and non-exclusive corner notifications
// (LayerShellNotificationSurface); neither fits a single full-output
// exclusive scrim, and duplicating ~30 lines here is cheaper and lower-risk
// than adding a third role to a module other lanes touch concurrently. If a
// second consumer needs the same shape (the session-lock slice, PF5-8, is
// the likely candidate), promote this into shell_surface then.
class PolkitOverlaySurface final {
public:
    // Configures `window` on the output the pointer is currently over
    // (falling back to the primary screen, exactly as
    // GatherOverviewComposition::screenToOpenOn() does, and for the same
    // reason: a layer-shell client is never told which output has focus).
    // Must be called before the window is first shown. Returns false and
    // leaves it hidden when Wayland or its layer-shell role is unavailable.
    [[nodiscard]] static bool configure(QQuickWindow &window, QString *error = nullptr);
};

} // namespace QindaQt::Apps::PolkitAgent
