// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// Seam for an authenticated compositor-owned active-window observer. A later
// milestone supplies a real adapter (analogous to SessionLockState's
// owner/PID-authenticated KWin observation); G0 tests exercise only fakes.
class ActiveWindowSource
{
public:
    virtual ~ActiveWindowSource() = default;

    // AGENT-CONTRACT: implementations must derive `processId` from an
    // authenticated compositor-owned inventory, never from unauthenticated
    // client-supplied metadata. Returns std::nullopt when no window is
    // active or the source is not yet authenticated; callers must treat that
    // as "no provider can be authenticated right now."
    [[nodiscard]] virtual std::optional<WindowIdentity> activeWindow() const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
